// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderingThread.cpp: Rendering thread implementation.
=============================================================================*/

#include "RenderCorePrivatePCH.h"
#include "RenderCore.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "TickableObjectRenderThread.h"
#include "ExceptionHandling.h"
#include "TaskGraphInterfaces.h"
#include "StatsData.h"

//
// Globals
//



RENDERCORE_API bool GIsThreadedRendering = false;
RENDERCORE_API bool GUseThreadedRendering = false;
RENDERCORE_API bool GUseRHIThread = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	RENDERCORE_API bool GMainThreadBlockedOnRenderThread = false;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static FRunnable* GRenderingThreadRunnable = NULL;

/** If the rendering thread has been terminated by an unhandled exception, this contains the error message. */
FString GRenderingThreadError;

/**
 * Polled by the game thread to detect crashes in the rendering thread.
 * If the rendering thread crashes, it sets this variable to false.
 */
volatile bool GIsRenderingThreadHealthy = true;


/**
 * Maximum rate the rendering thread will tick tickables when idle (in Hz)
 */
float GRenderingThreadMaxIdleTickFrequency = 40.f;

/** Function to stall the rendering thread **/
static void SuspendRendering()
{
	FPlatformAtomics::InterlockedIncrement(&GIsRenderingThreadSuspended);
	FPlatformMisc::MemoryBarrier();
}

/** Function to wait and resume rendering thread **/
static void WaitAndResumeRendering()
{
	while ( GIsRenderingThreadSuspended )
	{
		// Just sleep a little bit.
		FPlatformProcess::Sleep( 0.001f ); //@todo this should be a more principled wait
	}
    
	// set the thread back to real time mode
	FPlatformProcess::SetRealTimeMode();
}

/**
 *	Constructor that flushes and suspends the renderthread
 *	@param bRecreateThread	- Whether the rendering thread should be completely destroyed and recreated, or just suspended.
 */
FSuspendRenderingThread::FSuspendRenderingThread( bool bInRecreateThread )
{
	// Suspend async loading thread so that it doesn't start queueing render commands 
	// while the render thread is suspended.
	if (IsAsyncLoadingMultithreaded())
	{
		SuspendAsyncLoading();
	}

	bRecreateThread = bInRecreateThread;
	bUseRenderingThread = GUseThreadedRendering;
	bWasRenderingThreadRunning = GIsThreadedRendering;
	if ( bRecreateThread )
	{
		StopRenderingThread();
		// GUseThreadedRendering should be set to false after StopRenderingThread call since
		// otherwise a wrong context could be used.
		GUseThreadedRendering = false;
		FPlatformAtomics::InterlockedIncrement( &GIsRenderingThreadSuspended );
	}
	else
	{
		if ( GIsRenderingThreadSuspended == 0 )
		{
			// First tell the render thread to finish up all pending commands and then suspend its activities.
			
			// this ensures that async stuff will be completed too
			FlushRenderingCommands();
			
			if (GIsThreadedRendering)
			{
				DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SuspendRendering"),
					STAT_FSimpleDelegateGraphTask_SuspendRendering,
					STATGROUP_TaskGraphTasks);

				FGraphEventRef CompleteHandle = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateStatic(&SuspendRendering),
					GET_STATID(STAT_FSimpleDelegateGraphTask_SuspendRendering), NULL, ENamedThreads::RenderThread);

				// Busy wait while Kismet debugging, to avoid opportunistic execution of game thread tasks
				// If the game thread is already executing tasks, then we have no choice but to spin
				if (GIntraFrameDebuggingGameThread || FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread) ) 
				{
					while (!GIsRenderingThreadSuspended)
					{
						FPlatformProcess::Sleep(0.0f);
					}
				}
				else
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_FSuspendRenderingThread);
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompleteHandle, ENamedThreads::GameThread);
				}
				check(GIsRenderingThreadSuspended);
			
				// Now tell the render thread to busy wait until it's resumed
				DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.WaitAndResumeRendering"),
					STAT_FSimpleDelegateGraphTask_WaitAndResumeRendering,
					STATGROUP_TaskGraphTasks);

				FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
					FSimpleDelegateGraphTask::FDelegate::CreateStatic(&WaitAndResumeRendering),
					GET_STATID(STAT_FSimpleDelegateGraphTask_WaitAndResumeRendering), NULL, ENamedThreads::RenderThread);
			}
			else
			{
				SuspendRendering();
			}
		}
		else
		{
			// The render-thread is already suspended. Just bump the ref-count.
			FPlatformAtomics::InterlockedIncrement( &GIsRenderingThreadSuspended );
		}
	}
}

/** Destructor that starts the renderthread again */
FSuspendRenderingThread::~FSuspendRenderingThread()
{
#if PLATFORM_MAC	// On OS X Apple's context sharing is a strict interpretation of the spec. so a resource is only properly visible to other contexts
					// in the share group after a flush. Thus we call RHIFlushResources which will flush the current context's commands to GL (but not wait for them).
	ENQUEUE_UNIQUE_RENDER_COMMAND(FlushCommand,
		RHIFlushResources();
	);
#endif
	
	if ( bRecreateThread )
	{
		GUseThreadedRendering = bUseRenderingThread;
		FPlatformAtomics::InterlockedDecrement( &GIsRenderingThreadSuspended );
		if ( bUseRenderingThread && bWasRenderingThreadRunning )
		{
			StartRenderingThread();
            
            // Now tell the render thread to set it self to real time mode
			DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SetRealTimeMode"),
				STAT_FSimpleDelegateGraphTask_SetRealTimeMode,
				STATGROUP_TaskGraphTasks);

            FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
                FSimpleDelegateGraphTask::FDelegate::CreateStatic(&FPlatformProcess::SetRealTimeMode),
				GET_STATID(STAT_FSimpleDelegateGraphTask_SetRealTimeMode), NULL, ENamedThreads::RenderThread
			);
        }
	}
	else
	{
		// Resume the render thread again. 
		FPlatformAtomics::InterlockedDecrement( &GIsRenderingThreadSuspended );
	}
	if (IsAsyncLoadingMultithreaded())
	{
		ResumeAsyncLoading();
	}
}


/**
 * Tick all rendering thread tickable objects
 */

/** Static array of tickable objects that are ticked from rendering thread*/
FTickableObjectRenderThread::FRenderingThreadTickableObjectsArray FTickableObjectRenderThread::RenderingThreadTickableObjects;
FTickableObjectRenderThread::FRenderingThreadTickableObjectsArray FTickableObjectRenderThread::RenderingThreadHighFrequencyTickableObjects;

void TickHighFrequencyTickables(double CurTime)
{
	static double LastHighFreqTime = FPlatformTime::Seconds();
	float DeltaSecondsHighFreq = CurTime - LastHighFreqTime;

	// tick any high frequency rendering thread tickables.
	for (int32 ObjectIndex = 0; ObjectIndex < FTickableObjectRenderThread::RenderingThreadHighFrequencyTickableObjects.Num(); ObjectIndex++)
	{
		FTickableObjectRenderThread* TickableObject = FTickableObjectRenderThread::RenderingThreadHighFrequencyTickableObjects[ObjectIndex];
		// make sure it wants to be ticked and the rendering thread isn't suspended
		if (TickableObject->IsTickable())
		{
			STAT(FScopeCycleCounter(TickableObject->GetStatId());)
				TickableObject->Tick(DeltaSecondsHighFreq);
		}
	}

	LastHighFreqTime = CurTime;
}

void TickRenderingTickables()
{
	static double LastTickTime = FPlatformTime::Seconds();
	

	// calc how long has passed since last tick
	double CurTime = FPlatformTime::Seconds();
	float DeltaSeconds = CurTime - LastTickTime;
		
	TickHighFrequencyTickables(CurTime);

	if (DeltaSeconds < (1.f/GRenderingThreadMaxIdleTickFrequency))
	{
		return;
	}

	// tick any rendering thread tickables
	for (int32 ObjectIndex = 0; ObjectIndex < FTickableObjectRenderThread::RenderingThreadTickableObjects.Num(); ObjectIndex++)
	{
		FTickableObjectRenderThread* TickableObject = FTickableObjectRenderThread::RenderingThreadTickableObjects[ObjectIndex];
		// make sure it wants to be ticked and the rendering thread isn't suspended
		if (TickableObject->IsTickable())
		{
			STAT(FScopeCycleCounter(TickableObject->GetStatId());)
			TickableObject->Tick(DeltaSeconds);
		}
	}
	// update the last time we ticked
	LastTickTime = CurTime;
}

/** Accumulates how many cycles the renderthread has been idle. It's defined in RenderingThread.cpp. */
uint32 GRenderThreadIdle[ERenderThreadIdleTypes::Num] = {0};
/** Accumulates how times renderthread was idle. It's defined in RenderingThread.cpp. */
uint32 GRenderThreadNumIdle[ERenderThreadIdleTypes::Num] = {0};
/** How many cycles the renderthread used (excluding idle time). It's set once per frame in FViewport::Draw. */
uint32 GRenderThreadTime = 0;



/** The RHI thread runnable object. */
class FRHIThread : public FRunnable
{
public:
	FRunnableThread* Thread;

	FRHIThread()
		: Thread(nullptr)
	{
		check(IsInGameThread());
	}

	virtual uint32 Run() override
	{
		FMemory::SetupTLSCachesOnCurrentThread();
		FTaskGraphInterface::Get().AttachToThread(ENamedThreads::RHIThread);
		FTaskGraphInterface::Get().ProcessThreadUntilRequestReturn(ENamedThreads::RHIThread);
		FMemory::ClearAndDisableTLSCachesOnCurrentThread();
		return 0;
	}

	static FRHIThread& Get()
	{
		static FRHIThread Singleton;
		return Singleton;
	}

	void Start()
	{
		Thread = FRunnableThread::Create(this, TEXT("RHIThread"), 512 * 1024, TPri_SlightlyBelowNormal, 
			FPlatformAffinity::GetRHIThreadMask()
			);
		check(Thread);
	}
};

/** The rendering thread main loop */
void RenderingThreadMain( FEvent* TaskGraphBoundSyncEvent )
{
	ENamedThreads::RenderThread = ENamedThreads::Type(ENamedThreads::ActualRenderingThread);
	ENamedThreads::RenderThread_Local = ENamedThreads::Type(ENamedThreads::ActualRenderingThread_Local);
	FTaskGraphInterface::Get().AttachToThread(ENamedThreads::RenderThread);
	FPlatformMisc::MemoryBarrier();

	// Inform main thread that the render thread has been attached to the taskgraph and is ready to receive tasks
	if( TaskGraphBoundSyncEvent != NULL )
	{
		TaskGraphBoundSyncEvent->Trigger();
	}

	// set the thread back to real time mode
	FPlatformProcess::SetRealTimeMode();

#if STATS
	if (FThreadStats::WillEverCollectData())
	{
		FThreadStats::ExplicitFlush(); // flush the stats and set update the scope so we don't flush again until a frame update, this helps prevent fragmentation
	}
#endif

	FCoreDelegates::PostRenderingThreadCreated.Broadcast();
	check(GIsThreadedRendering);
	FTaskGraphInterface::Get().ProcessThreadUntilRequestReturn(ENamedThreads::RenderThread);
	FPlatformMisc::MemoryBarrier();
	check(!GIsThreadedRendering);
	FCoreDelegates::PreRenderingThreadDestroyed.Broadcast();
	
#if STATS
	if (FThreadStats::WillEverCollectData())
	{
		FThreadStats::ExplicitFlush(); // Another explicit flush to clean up the ScopeCount established above for any stats lingering since the last frame
	}
#endif
	
	ENamedThreads::RenderThread = ENamedThreads::GameThread;
	ENamedThreads::RenderThread_Local = ENamedThreads::GameThread_Local;
	FPlatformMisc::MemoryBarrier();
}

/**
 * Advances stats for the rendering thread.
 */
static void AdvanceRenderingThreadStats(int64 StatsFrame, int32 MasterDisableChangeTagStartFrame)
{
#if STATS
	int64 Frame = StatsFrame;
	if (!FThreadStats::IsCollectingData() ||  MasterDisableChangeTagStartFrame != FThreadStats::MasterDisableChangeTag())
	{
		Frame = -StatsFrame; // mark this as a bad frame
	}
	FThreadStats::AddMessage(FStatConstants::AdvanceFrame.GetEncodedName(), EStatOperation::AdvanceFrameEventRenderThread, Frame);
	if( IsInActualRenderingThread() )
	{
		FThreadStats::ExplicitFlush();
	}
#endif
}

/**
 * Advances stats for the rendering thread. Called from the game thread.
 */
void AdvanceRenderingThreadStatsGT( bool bDiscardCallstack, int64 StatsFrame, int32 MasterDisableChangeTagStartFrame )
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER
	(
		RenderingThreadTickCommand,
		int64, SentStatsFrame, StatsFrame,
		int32, SentMasterDisableChangeTagStartFrame, MasterDisableChangeTagStartFrame,
		{
			AdvanceRenderingThreadStats( SentStatsFrame, SentMasterDisableChangeTagStartFrame );
		}
	);
	if( bDiscardCallstack )
	{
		// we need to flush the rendering thread here, otherwise it can get behind and then the stats will get behind.
		FlushRenderingCommands();
	}
}

/** The rendering thread runnable object. */
class FRenderingThread : public FRunnable
{
private:
	/** Tracks if we have acquired ownership */
	bool bAcquiredThreadOwnership;

public:
	/** 
	 * Sync event to make sure that render thread is bound to the task graph before main thread queues work against it.
	 */
	FEvent* TaskGraphBoundSyncEvent;

	FRenderingThread()
	{
		bAcquiredThreadOwnership = false;
		TaskGraphBoundSyncEvent	= FPlatformProcess::GetSynchEventFromPool(true);
		RHIFlushResources();
	}

	virtual ~FRenderingThread()
	{
		FPlatformProcess::ReturnSynchEventToPool(TaskGraphBoundSyncEvent);
		TaskGraphBoundSyncEvent = nullptr;
	}

	// FRunnable interface.
	virtual bool Init(void) override
	{ 
		GRenderThreadId = FPlatformTLS::GetCurrentThreadId();

		// Acquire rendering context ownership on the current thread, unless using an RHI thread, which will be the real owner
		if (!GUseRHIThread)
		{
			bAcquiredThreadOwnership = true;
			RHIAcquireThreadOwnership();
		}

		return true; 
	}

	virtual void Exit(void) override
	{
		// Release rendering context ownership on the current thread if we had acquired it
		if (bAcquiredThreadOwnership)
		{
			bAcquiredThreadOwnership = false;
			RHIReleaseThreadOwnership();
		}

		GRenderThreadId = 0;
	}

#if PLATFORM_WINDOWS && !PLATFORM_SEH_EXCEPTIONS_DISABLED
	static int32 FlushRHILogsAndReportCrash(LPEXCEPTION_POINTERS ExceptionInfo)
	{
		if (GDynamicRHI)
		{
			GDynamicRHI->FlushPendingLogs();
		}

		return ReportCrash(ExceptionInfo);
	}
#endif

	virtual uint32 Run(void) override
	{
		FMemory::SetupTLSCachesOnCurrentThread();
		FPlatformProcess::SetupRenderThread();

#if PLATFORM_WINDOWS
		if ( !FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash )
		{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
			__try
#endif
			{
				RenderingThreadMain( TaskGraphBoundSyncEvent );
			}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
			__except(FlushRHILogsAndReportCrash(GetExceptionInformation()))
			{
				GRenderingThreadError = GErrorHist;

				// Use a memory barrier to ensure that the game thread sees the write to GRenderingThreadError before
				// the write to GIsRenderingThreadHealthy.
				FPlatformMisc::MemoryBarrier();

				GIsRenderingThreadHealthy = false;
			}
#endif
		}
		else
#endif // PLATFORM_WINDOWS
		{
			RenderingThreadMain( TaskGraphBoundSyncEvent );
		}
		FMemory::ClearAndDisableTLSCachesOnCurrentThread();
		return 0;
	}
};

/**
 * If the rendering thread is in its idle loop (which ticks rendering tickables
 */
volatile bool GRunRenderingThreadHeartbeat = false;

FThreadSafeCounter OutstandingHeartbeats;
/** The rendering thread heartbeat runnable object. */
class FRenderingThreadTickHeartbeat : public FRunnable
{
public:

	// FRunnable interface.
	virtual bool Init(void) 
	{ 
		OutstandingHeartbeats.Reset();
		return true; 
	}

	virtual void Exit(void) 
	{
	}

	virtual void Stop(void)
	{
	}

	virtual uint32 Run(void)
	{
		while(GRunRenderingThreadHeartbeat)
		{
			FPlatformProcess::Sleep(1.f/(4.0f * GRenderingThreadMaxIdleTickFrequency));
			if (!GIsRenderingThreadSuspended && OutstandingHeartbeats.GetValue() < 4)
			{
				OutstandingHeartbeats.Increment();
				ENQUEUE_UNIQUE_RENDER_COMMAND(
					HeartbeatTickTickables,
				{
					OutstandingHeartbeats.Decrement();
					// make sure that rendering thread tickables get a chance to tick, even if the render thread is starving
					if (!GIsRenderingThreadSuspended)
					{
						TickRenderingTickables();
					}
				});
			}
		}
		return 0;
	}
};

FRunnableThread* GRenderingThreadHeartbeat = NULL;
FRunnable* GRenderingThreadRunnableHeartbeat = NULL;

// not done in the CVar system as we don't access to render thread specifics there
struct FConsoleRenderThreadPropagation : public IConsoleThreadPropagation
{
	virtual void OnCVarChange(int32& Dest, int32 NewValue)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			OnCVarChange1,
			int32&, Dest, Dest,
			int32, NewValue, NewValue,
		{
			Dest = NewValue;
		});
	}
	
	virtual void OnCVarChange(float& Dest, float NewValue)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			OnCVarChange2,
			float&, Dest, Dest,
			float, NewValue, NewValue,
		{
			Dest = NewValue;
		});
	}

	virtual void OnCVarChange(bool& Dest, bool NewValue)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			OnCVarChange2,
			bool&, Dest, Dest,
			bool, NewValue, NewValue,
		{
			Dest = NewValue;
		});
	}
	
	virtual void OnCVarChange(FString& Dest, const FString& NewValue)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			OnCVarChange3,
			FString&, Dest, Dest,
			const FString&, NewValue, NewValue,
		{
			Dest = NewValue;
		});
	}

	static FConsoleRenderThreadPropagation& GetSingleton()
	{
		static FConsoleRenderThreadPropagation This;

		return This;
	}

};

static FString BuildRenderingThreadName( uint32 ThreadIndex )
{
	return FString::Printf( TEXT( "%s %u" ), *FName( NAME_RenderThread ).GetPlainNameString(), ThreadIndex );
}



class FOwnershipOfRHIThreadTask : public FCustomStatIDGraphTaskBase
{
public:
	/**
	*	Constructor
	*	@param StatId The stat id for this task.
	*	@param InDesiredThread; Thread to run on, can be ENamedThreads::AnyThread
	**/
	FOwnershipOfRHIThreadTask(bool bInAcquireOwnership, TStatId StatId)
		: FCustomStatIDGraphTaskBase(StatId)
		, bAcquireOwnership(bInAcquireOwnership)
	{
	}

	/**
	*	Retrieve the thread that this task wants to run on.
	*	@return the thread that this task should run on.
	**/
	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::RHIThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	/**
	*	Actually execute the task.
	*	@param	CurrentThread; the thread we are running on
	*	@param	MyCompletionGraphEvent; my completion event. Not always useful since at the end of DoWork, you can assume you are done and hence further tasks do not need you as a prerequisite.
	*	However, MyCompletionGraphEvent can be useful for passing to other routines or when it is handy to set up subsequents before you actually do work.
	**/
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		// note that this task is the first task run on the thread, before GRHIThread is assigned, so we can't check IsInRHIThread()

		if (bAcquireOwnership)
		{
			GDynamicRHI->RHIAcquireThreadOwnership();
		}
		else
		{
			GDynamicRHI->RHIReleaseThreadOwnership();
		}
	}

private:
	bool bAcquireOwnership;
};



void StartRenderingThread()
{
	static uint32 ThreadCount = 0;
	check(!GIsThreadedRendering && GUseThreadedRendering);

	check(!GRHIThread)
	if (GUseRHIThread)
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);		
		if (!FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::RHIThread))
		{
			FRHIThread::Get().Start();
		}
		DECLARE_CYCLE_STAT(TEXT("Wait For RHIThread"), STAT_WaitForRHIThread, STATGROUP_TaskGraphTasks);

		FGraphEventRef CompletionEvent = TGraphTask<FOwnershipOfRHIThreadTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(true, GET_STATID(STAT_WaitForRHIThread));
		QUICK_SCOPE_CYCLE_COUNTER(STAT_StartRenderingThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompletionEvent, ENamedThreads::GameThread_Local);
		GRHIThread = FRHIThread::Get().Thread;
		check(GRHIThread);
		GRHICommandList.LatchBypass();
	}

	// Turn on the threaded rendering flag.
	GIsThreadedRendering = true;

	// Create the rendering thread.
	GRenderingThreadRunnable = new FRenderingThread();

	GRenderingThread = FRunnableThread::Create(GRenderingThreadRunnable, *BuildRenderingThreadName(ThreadCount), 0, FPlatformAffinity::GetRenderingThreadPriority(), FPlatformAffinity::GetRenderingThreadMask());

	// Wait for render thread to have taskgraph bound before we dispatch any tasks for it.
	((FRenderingThread*)GRenderingThreadRunnable)->TaskGraphBoundSyncEvent->Wait();

	// register
	IConsoleManager::Get().RegisterThreadPropagation(GRenderingThread->GetThreadID(), &FConsoleRenderThreadPropagation::GetSingleton());

	// ensure the thread has actually started and is idling
	FRenderCommandFence Fence;
	Fence.BeginFence();
	Fence.Wait();

	GRunRenderingThreadHeartbeat = true;
	// Create the rendering thread heartbeat
	GRenderingThreadRunnableHeartbeat = new FRenderingThreadTickHeartbeat();

	GRenderingThreadHeartbeat = FRunnableThread::Create(GRenderingThreadRunnableHeartbeat, *FString::Printf(TEXT("RTHeartBeat %d"), ThreadCount), 16 * 1024, TPri_AboveNormal, FPlatformAffinity::GetRTHeartBeatMask());

	ThreadCount++;
}


void StopRenderingThread()
{
	// This function is not thread-safe. Ensure it is only called by the main game thread.
	check( IsInGameThread() );
	
	// unregister
	IConsoleManager::Get().RegisterThreadPropagation();

	// stop the render thread heartbeat first
	if (GRunRenderingThreadHeartbeat)
	{
		GRunRenderingThreadHeartbeat = false;
		// Wait for the rendering thread heartbeat to return.
		GRenderingThreadHeartbeat->WaitForCompletion();
		delete GRenderingThreadHeartbeat;
		GRenderingThreadHeartbeat = NULL;
		delete GRenderingThreadRunnableHeartbeat;
		GRenderingThreadRunnableHeartbeat = NULL;
	}

	if( GIsThreadedRendering )
	{
		// Get the list of objects which need to be cleaned up when the rendering thread is done with them.
		FPendingCleanupObjects* PendingCleanupObjects = GetPendingCleanupObjects();

		// Make sure we're not in the middle of streaming textures.
		(*GFlushStreamingFunc)();

		// Wait for the rendering thread to finish executing all enqueued commands.
		FlushRenderingCommands();

		// The rendering thread may have already been stopped during the call to GFlushStreamingFunc or FlushRenderingCommands.
		if ( GIsThreadedRendering )
		{
			if (GRHIThread)
			{
				DECLARE_CYCLE_STAT(TEXT("Wait For RHIThread Finish"), STAT_WaitForRHIThreadFinish, STATGROUP_TaskGraphTasks);
				FGraphEventRef ReleaseTask = TGraphTask<FOwnershipOfRHIThreadTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(false, GET_STATID(STAT_WaitForRHIThreadFinish));
				QUICK_SCOPE_CYCLE_COUNTER(STAT_StopRenderingThread_RHIThread);
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(ReleaseTask, ENamedThreads::GameThread_Local);
				GRHIThread = nullptr;
			}

			check( GRenderingThread );
			check(!GIsRenderingThreadSuspended);

			// Turn off the threaded rendering flag.
			GIsThreadedRendering = false;

			{
				FGraphEventRef QuitTask = TGraphTask<FReturnGraphTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(ENamedThreads::RenderThread);

				// Busy wait while BP debugging, to avoid opportunistic execution of game thread tasks
				// If the game thread is already executing tasks, then we have no choice but to spin
				if (GIntraFrameDebuggingGameThread || FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread) ) 
				{
					while ((QuitTask.GetReference() != nullptr) && !QuitTask->IsComplete())
					{
						FPlatformProcess::Sleep(0.0f);
					}
				}
				else
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_StopRenderingThread);
					FTaskGraphInterface::Get().WaitUntilTaskCompletes(QuitTask, ENamedThreads::GameThread_Local);
				}
			}

			// Wait for the rendering thread to return.
			GRenderingThread->WaitForCompletion();

			// Destroy the rendering thread objects.
			delete GRenderingThread;
			GRenderingThread = NULL;
			
			GRHICommandList.LatchBypass();

			delete GRenderingThreadRunnable;
			GRenderingThreadRunnable = NULL;
		}

		// Delete the pending cleanup objects which were in use by the rendering thread.
		delete PendingCleanupObjects;
	}

	check(!GRHIThread);
}

void CheckRenderingThreadHealth()
{
	if(!GIsRenderingThreadHealthy)
	{
		GErrorHist[0] = 0;
		GIsCriticalError = false;
		UE_LOG(LogRendererCore, Fatal,TEXT("Rendering thread exception:\r\n%s"),*GRenderingThreadError);
	}
	

	if (IsInGameThread())
	{
		if (!GIsCriticalError)
		{
			GLog->FlushThreadedLogs();
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		TGuardValue<bool> GuardMainThreadBlockedOnRenderThread(GMainThreadBlockedOnRenderThread,true);
#endif
		SCOPE_CYCLE_COUNTER(STAT_PumpMessages);
		FPlatformMisc::PumpMessages(false);
	}
}

bool IsRenderingThreadHealthy()
{
	return GIsRenderingThreadHealthy;
}


void FRenderCommandFence::BeginFence()
{
	if (!GIsThreadedRendering)
	{
		return;
	}
	else
	{
		DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.FenceRenderCommand"),
			STAT_FNullGraphTask_FenceRenderCommand,
			STATGROUP_TaskGraphTasks);

		CompletionEvent = TGraphTask<FNullGraphTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(
			GET_STATID(STAT_FNullGraphTask_FenceRenderCommand), ENamedThreads::RenderThread);
	}
}

bool FRenderCommandFence::IsFenceComplete() const
{
	if (!GIsThreadedRendering)
	{
		return true;
	}
	check(IsInGameThread() || IsInAsyncLoadingThread());
	CheckRenderingThreadHealth();
	if (!CompletionEvent.GetReference() || CompletionEvent->IsComplete())
	{
		CompletionEvent = NULL; // this frees the handle for other uses, the NULL state is considered completed
		return true;
	}
	return false;
}

/** How many cycles the gamethread used (excluding idle time). It's set once per frame in FViewport::Draw. */
uint32 GGameThreadTime = 0;
/** How many cycles it took to swap buffers to present the frame. */
uint32 GSwapBufferTime = 0;

static int32 GTimeToBlockOnRenderFence = 1;
static FAutoConsoleVariableRef CVarTimeToBlockOnRenderFence(
	TEXT("g.TimeToBlockOnRenderFence"),
	GTimeToBlockOnRenderFence,
	TEXT("Number of milliseconds the game thread should block when waiting on a render thread fence.")
	);

/**
 * Block the game thread waiting for a task to finish on the rendering thread.
 */
static void GameThreadWaitForTask(const FGraphEventRef& Task, bool bEmptyGameThreadTasks = false)
{
	SCOPE_TIME_GUARD(TEXT("GameThreadWaitForTask"));

	check(IsInGameThread());
	check(IsValidRef(Task));	

	if (!Task->IsComplete())
	{
		SCOPE_CYCLE_COUNTER(STAT_GameIdleTime);
		{
			static int32 NumRecursiveCalls = 0;
		
			// Check for recursion. It's not completely safe but because we pump messages while 
			// blocked it is expected.
			NumRecursiveCalls++;
			if (NumRecursiveCalls > 1)
			{
				UE_LOG(LogRendererCore,Warning,TEXT("FlushRenderingCommands called recursively! %d calls on the stack."), NumRecursiveCalls);
			}
			if (NumRecursiveCalls > 1 || FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread))
			{
				bEmptyGameThreadTasks = false; // we don't do this on recursive calls or if we are at a blueprint breakpoint
			}

			// Grab an event from the pool and fire off a task to trigger it.
			FEvent* Event = FPlatformProcess::GetSynchEventFromPool();
			FTaskGraphInterface::Get().TriggerEventWhenTaskCompletes(Event, Task, ENamedThreads::GameThread);

			// Check rendering thread health needs to be called from time to
			// time in order to pump messages, otherwise the RHI may block
			// on vsync causing a deadlock. Also we should make sure the
			// rendering thread hasn't crashed :)
			bool bDone;
			uint32 WaitTime = FMath::Clamp<uint32>(GTimeToBlockOnRenderFence, 0, 33);
			do
			{
				CheckRenderingThreadHealth();
				if (bEmptyGameThreadTasks)
				{
					// process gamethread tasks if there are any
					FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
				}
				bDone = Event->Wait(WaitTime);
			}
			while (!bDone);

			// Return the event to the pool and decrement the recursion counter.
			FPlatformProcess::ReturnSynchEventToPool(Event);
			Event = nullptr;
			NumRecursiveCalls--;
		}
	}
}

/**
 * Waits for pending fence commands to retire.
 */
void FRenderCommandFence::Wait(bool bProcessGameThreadTasks) const
{
	if (!IsFenceComplete())
	{
#if 0
		// on most platforms this is a better solution because it doesn't spin
		// windows needs to pump messages
		if (bProcessGameThreadTasks)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FRenderCommandFence_Wait);
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(CompletionEvent, ENamedThreads::GameThread);
		}
#endif
		GameThreadWaitForTask(CompletionEvent, bProcessGameThreadTasks);
	}
}

/**
 * List of tasks that must be completed before we start a render frame
 * Note, normally, you don't need the render command themselves in this list workers that queue render commands are usually sufficient
 */
static FCompletionList FrameRenderPrerequisites;

/**
 * Adds a task that must be completed either before the next scene draw or a flush rendering commands
 * Note, normally, you don't need the render command themselves in this list workers that queue render commands are usually sufficient
 * @param TaskToAdd, task to add as a pending render thread task
 */
void AddFrameRenderPrerequisite(const FGraphEventRef& TaskToAdd)
{
	FrameRenderPrerequisites.Add(TaskToAdd);
}

/**
 * Gather the frame render prerequisites and make sure all render commands are at least queued
 */
void AdvanceFrameRenderPrerequisite()
{
	checkSlow(IsInGameThread()); 
	FGraphEventRef PendingComplete = FrameRenderPrerequisites.CreatePrerequisiteCompletionHandle(ENamedThreads::GameThread);
	if (PendingComplete.GetReference())
	{
		GameThreadWaitForTask(PendingComplete);
	}
}


/**
 * Waits for the rendering thread to finish executing all pending rendering commands.  Should only be used from the game thread.
 */
void FlushRenderingCommands()
{
	if (!GIsRHIInitialized)
	{
		return;
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND(
		FlushPendingDeleteRHIResources,
	{
		GRHICommandList.GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
	}
	);

	AdvanceFrameRenderPrerequisite();

	// Find the objects which may be cleaned up once the rendering thread command queue has been flushed.
	FPendingCleanupObjects* PendingCleanupObjects = GetPendingCleanupObjects();

	// Issue a fence command to the rendering thread and wait for it to complete.
	FRenderCommandFence Fence;
	Fence.BeginFence();
	Fence.Wait();

	// Delete the objects which were enqueued for deferred cleanup before the command queue flush.
	delete PendingCleanupObjects;
}

void FlushPendingDeleteRHIResources_GameThread()
{
	if (!GRHIThread)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			FlushPendingDeleteRHIResources,
		{
			FlushPendingDeleteRHIResources_RenderThread();
		}
		);
	}
}
void FlushPendingDeleteRHIResources_RenderThread()
{
	if (!GRHIThread)
	{
		FRHIResource::FlushPendingDeletes();
	}
}


FRHICommandListImmediate& GetImmediateCommandList_ForRenderCommand()
{
	return FRHICommandListExecutor::GetImmediateCommandList();
}

/** The set of deferred cleanup objects which are pending cleanup. */
static TLockFreePointerListUnordered<FDeferredCleanupInterface, PLATFORM_CACHE_LINE_SIZE>	PendingCleanupObjectsList;

FPendingCleanupObjects::FPendingCleanupObjects()
{
	check(IsInGameThread());
	PendingCleanupObjectsList.PopAll(CleanupArray);
}

FPendingCleanupObjects::~FPendingCleanupObjects()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FPendingCleanupObjects_Destruct);

	for(int32 ObjectIndex = 0;ObjectIndex < CleanupArray.Num();ObjectIndex++)
	{
		CleanupArray[ObjectIndex]->FinishCleanup();
	}
}

void BeginCleanup(FDeferredCleanupInterface* CleanupObject)
{
	PendingCleanupObjectsList.Push(CleanupObject);
}

FPendingCleanupObjects* GetPendingCleanupObjects()
{
	return new FPendingCleanupObjects;
}

void SetRHIThreadEnabled(bool bEnable)
{
	if (bEnable != GUseRHIThread)
	{
		if (GRHISupportsRHIThread)
		{
			if (!GIsThreadedRendering)
			{
				check(!GRHIThread);
				UE_LOG(LogRendererCore, Display, TEXT("Can't switch to RHI thread mode when we are not running a multithreaded renderer."));
			}
			else
			{
				StopRenderingThread();
				GUseRHIThread = bEnable;
				StartRenderingThread();
			}
			UE_LOG(LogRendererCore, Display, TEXT("RHIThread is now %s."), GRHIThread ? TEXT("active") : TEXT("inactive"));
		}
		else
		{
			UE_LOG(LogRendererCore, Display, TEXT("This RHI does not support the RHI thread."));
		}
	}
}

static void HandleRHIThreadEnableChanged(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		const bool bUseRHIThread = Args[0].ToBool();
		SetRHIThreadEnabled(bUseRHIThread);
	}
	else
	{
		UE_LOG(LogRendererCore, Display, TEXT("Usage: r.RHIThread.Enable 0/1; Currently %d"), (int32)GUseRHIThread);
	}
}

static FAutoConsoleCommand CVarRHIThreadEnable(
	TEXT("r.RHIThread.Enable"),
	TEXT("Enables/disabled the RHI Thread\n"),	
	FConsoleCommandWithArgsDelegate::CreateStatic(&HandleRHIThreadEnableChanged)
	);
