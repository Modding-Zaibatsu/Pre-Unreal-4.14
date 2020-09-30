// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsPublic.h"
#include "ParticleDefinitions.h"
#include "PrecomputedLightVolume.h"

#if WITH_PHYSX
	#include "PhysXSupport.h"
#endif

#if WITH_BOX2D
	#include "../PhysicsEngine2D/Box2DIntegration.h"
#endif
#include "PhysicsEngine/PhysicsSettings.h"

#ifndef APEX_STATICALLY_LINKED
	#define APEX_STATICALLY_LINKED	0
#endif


FPhysCommandHandler * GPhysCommandHandler = NULL;
FDelegateHandle GPreGarbageCollectDelegateHandle;

// CVars
static TAutoConsoleVariable<float> CVarToleranceScaleLength(
	TEXT("p.ToleranceScale_Length"),
	100.f,
	TEXT("The approximate size of objects in the simulation. Default: 100"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarToleranceScaleSpeed(
	TEXT("p.ToleranceScale_Speed"),
	1000.f,
	TEXT("The typical magnitude of velocities of objects in simulation. Default: 1000"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarAPEXMaxDestructibleDynamicChunkIslandCount(
	TEXT("p.APEXMaxDestructibleDynamicChunkIslandCount"),
	2000,
	TEXT("APEX Max Destructilbe Dynamic Chunk Island Count."),
	ECVF_Default);


static TAutoConsoleVariable<int32> CVarAPEXMaxDestructibleDynamicChunkCount(
	TEXT("p.APEXMaxDestructibleDynamicChunkCount"),
	2000,
	TEXT("APEX Max Destructible dynamic Chunk Count."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAPEXSortDynamicChunksByBenefit(
	TEXT("p.bAPEXSortDynamicChunksByBenefit"),
	1,
	TEXT("True if APEX should sort dynamic chunks by benefit."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarUseUnifiedHeightfield(
	TEXT("p.bUseUnifiedHeightfield"),
	1,
	TEXT("Whether to use the PhysX unified heightfield. This feature of PhysX makes landscape collision consistent with triangle meshes but the thickness parameter is not supported for unified heightfields. 1 enables and 0 disables. Default: 1"),
	ECVF_ReadOnly);



//////////////////////////////////////////////////////////////////////////
// UWORLD
//////////////////////////////////////////////////////////////////////////

void UWorld::SetupPhysicsTickFunctions(float DeltaSeconds)
{
	StartPhysicsTickFunction.bCanEverTick = true;
	StartPhysicsTickFunction.Target = this;
	
	EndPhysicsTickFunction.bCanEverTick = true;
	EndPhysicsTickFunction.Target = this;
	
	StartAsyncTickFunction.bCanEverTick = true;
	StartAsyncTickFunction.Target = this;
	
	
	// see if we need to update tick registration
	bool bNeedToUpdateTickRegistration = (bShouldSimulatePhysics != StartPhysicsTickFunction.IsTickFunctionRegistered())
		|| (bShouldSimulatePhysics != EndPhysicsTickFunction.IsTickFunctionRegistered())
		|| (bShouldSimulatePhysics != StartAsyncTickFunction.IsTickFunctionRegistered());

	if (bNeedToUpdateTickRegistration && PersistentLevel)
	{
		if (bShouldSimulatePhysics && !StartPhysicsTickFunction.IsTickFunctionRegistered())
		{
			StartPhysicsTickFunction.TickGroup = TG_StartPhysics;
			StartPhysicsTickFunction.RegisterTickFunction(PersistentLevel);
		}
		else if (!bShouldSimulatePhysics && StartPhysicsTickFunction.IsTickFunctionRegistered())
		{
			StartPhysicsTickFunction.UnRegisterTickFunction();
		}

		if (bShouldSimulatePhysics && !EndPhysicsTickFunction.IsTickFunctionRegistered())
		{
			EndPhysicsTickFunction.TickGroup = TG_EndPhysics;
			EndPhysicsTickFunction.RegisterTickFunction(PersistentLevel);
			EndPhysicsTickFunction.AddPrerequisite(this, StartPhysicsTickFunction);
		}
		else if (!bShouldSimulatePhysics && EndPhysicsTickFunction.IsTickFunctionRegistered())
		{
			EndPhysicsTickFunction.RemovePrerequisite(this, StartPhysicsTickFunction);
			EndPhysicsTickFunction.UnRegisterTickFunction();
		}

		//async scene
		if (bShouldSimulatePhysics && !StartAsyncTickFunction.IsTickFunctionRegistered() && UPhysicsSettings::Get()->bEnableAsyncScene)
		{
			StartAsyncTickFunction.TickGroup = TG_EndPhysics;
			StartAsyncTickFunction.RegisterTickFunction(PersistentLevel);
			StartAsyncTickFunction.AddPrerequisite(this, EndPhysicsTickFunction);
		}
		else if (!bShouldSimulatePhysics && StartAsyncTickFunction.IsTickFunctionRegistered())
		{
			StartAsyncTickFunction.UnRegisterTickFunction();
		}
	}

	FPhysScene* PhysScene = GetPhysicsScene();
	if (PhysicsScene == NULL)
	{
		return;
	}

#if WITH_PHYSX
	
	// When ticking the main scene, clean up any physics engine resources (once a frame)
	DeferredPhysResourceCleanup();
#endif

	// Update gravity in case it changed
	FVector DefaultGravity( 0.f, 0.f, GetGravityZ() );

	static const auto CVar_MaxPhysicsDeltaTime = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("p.MaxPhysicsDeltaTime"));
	PhysScene->SetUpForFrame(&DefaultGravity, DeltaSeconds, UPhysicsSettings::Get()->MaxPhysicsDeltaTime);
}

void UWorld::StartPhysicsSim()
{
	FPhysScene* PhysScene = GetPhysicsScene();
	if (PhysScene == NULL)
	{
		return;
	}

	PhysScene->StartFrame();
}

void UWorld::FinishPhysicsSim()
{
	FPhysScene* PhysScene = GetPhysicsScene();
	if (PhysScene == NULL)
	{
		return;
	}

	PhysScene->EndFrame(LineBatcher);
}

void UWorld::StartAsyncSim()
{
	if (FPhysScene* PhysScene = GetPhysicsScene())
	{
		PhysScene->StartAsync();
	}
}

// the physics tick functions

void FStartPhysicsTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	QUICK_SCOPE_CYCLE_COUNTER(FStartPhysicsTickFunction_ExecuteTick);
	check(Target);
	Target->StartPhysicsSim();
}

FString FStartPhysicsTickFunction::DiagnosticMessage()
{
	return TEXT("FStartPhysicsTickFunction");
}

void FEndPhysicsTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	QUICK_SCOPE_CYCLE_COUNTER(FEndPhysicsTickFunction_ExecuteTick);

	check(Target);
	FPhysScene* PhysScene = Target->GetPhysicsScene();
	if (PhysScene == NULL)
	{
		return;
	}
	FGraphEventRef PhysicsComplete = PhysScene->GetCompletionEvent();
	if (PhysicsComplete.GetReference() && !PhysicsComplete->IsComplete())
	{
		// don't release the next tick group until the physics has completed and we have run FinishPhysicsSim
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.FinishPhysicsSim"),
			STAT_FSimpleDelegateGraphTask_FinishPhysicsSim,
			STATGROUP_TaskGraphTasks);

		MyCompletionGraphEvent->DontCompleteUntil(
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateUObject(Target, &UWorld::FinishPhysicsSim),
				GET_STATID(STAT_FSimpleDelegateGraphTask_FinishPhysicsSim), PhysicsComplete, ENamedThreads::GameThread
			)
		);
	}
	else
	{
		// it was already done, so let just do it.
		Target->FinishPhysicsSim();
	}

#if PHYSX_MEMORY_VALIDATION
	static int32 Frequency = 0;
	if (Frequency++ > 10)
	{
		Frequency = 0;
		GPhysXAllocator->ValidateHeaders();
	}
#endif
}

FString FEndPhysicsTickFunction::DiagnosticMessage()
{
	return TEXT("FEndPhysicsTickFunction");
}

void FStartAsyncSimulationFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	QUICK_SCOPE_CYCLE_COUNTER(FStartAsyncSimulationFunction_ExecuteTick);

	check(Target);
	Target->StartAsyncSim();
}

FString FStartAsyncSimulationFunction::DiagnosticMessage()
{
	return TEXT("FStartAsyncSimulationFunction");
}

void PvdConnect(FString Host, bool bVisualization);

//////// GAME-LEVEL RIGID BODY PHYSICS STUFF ///////
void InitGamePhys()
{
#if WITH_BOX2D
	FPhysicsIntegration2D::InitializePhysics();
#endif

#if WITH_PHYSX
	// Do nothing if SDK already exists
	if(GPhysXFoundation != NULL)
	{
		return;
	}

	// Make sure 
	LoadPhysXModules();

	// Create Foundation
	GPhysXAllocator = new FPhysXAllocator();
	FPhysXErrorCallback* ErrorCallback = new FPhysXErrorCallback();

	GPhysXFoundation = PxCreateFoundation(PX_FOUNDATION_VERSION, *GPhysXAllocator, *ErrorCallback);
	check(GPhysXFoundation);

#if PHYSX_MEMORY_STATS
	// Want names of PhysX allocations
	GPhysXFoundation->setReportAllocationNames(true);
#endif

	// Create profile manager
	GPhysXVisualDebugger = PxCreatePvd(*GPhysXFoundation);
	check(GPhysXVisualDebugger);

	// Create Physics
	PxTolerancesScale PScale;
	PScale.length = CVarToleranceScaleLength.GetValueOnGameThread();
	PScale.speed = CVarToleranceScaleSpeed.GetValueOnGameThread();

	GPhysXSDK = PxCreatePhysics(PX_PHYSICS_VERSION, *GPhysXFoundation, PScale, false, GPhysXVisualDebugger);
	check(GPhysXSDK);

	FPhysxSharedData::Initialize();

	GPhysCommandHandler = new FPhysCommandHandler();

	GPreGarbageCollectDelegateHandle = FCoreUObjectDelegates::PreGarbageCollect.AddRaw(GPhysCommandHandler, &FPhysCommandHandler::Flush);

	// Init Extensions
	PxInitExtensions(*GPhysXSDK, GPhysXVisualDebugger);
#if WITH_VEHICLE
	PxInitVehicleSDK(*GPhysXSDK);
#endif

	if (CVarUseUnifiedHeightfield.GetValueOnGameThread())
	{
		//Turn on PhysX 3.3 unified height field collision detection. 
		//This approach shares the collision detection code between meshes and height fields such that height fields behave identically to the equivalent terrain created as a mesh. 
		//This approach facilitates mixing the use of height fields and meshes in the application with no tangible difference in collision behavior between the two approaches except that 
		//heightfield thickness is not supported for unified heightfields.
		PxRegisterUnifiedHeightFields(*GPhysXSDK);
	}
	else
	{
		PxRegisterHeightFields(*GPhysXSDK);
	}

	if( FParse::Param( FCommandLine::Get(), TEXT( "PVD" ) ) )
	{
		PvdConnect(TEXT("localhost"), true);
	}


#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
	// Create Cooking
	PxCookingParams PCookingParams(PScale);
	PCookingParams.meshWeldTolerance = 0.1f; // Weld to 1mm precision
	PCookingParams.meshPreprocessParams = PxMeshPreprocessingFlags(PxMeshPreprocessingFlag::eWELD_VERTICES);
	// Force any cooking in PhysX or APEX to use older incremental hull method
	// This is because the new 'quick hull' method can generate degenerate geometry in some cases (very thin meshes etc.)
	//PCookingParams.convexMeshCookingType = PxConvexMeshCookingType::eINFLATION_INCREMENTAL_HULL;
	PCookingParams.targetPlatform = PxPlatform::ePC;
	//PCookingParams.meshCookingHint = PxMeshCookingHint::eCOOKING_PERFORMANCE;
	//PCookingParams.meshSizePerformanceTradeOff = 0.0f;
	GPhysXCooking = PxCreateCooking(PX_PHYSICS_VERSION, *GPhysXFoundation, PCookingParams);
	check(GPhysXCooking);
#endif

#if WITH_APEX
	// Build the descriptor for the APEX SDK
	apex::ApexSDKDesc ApexDesc;
	ApexDesc.foundation				= GPhysXFoundation;	// Pointer to the PxFoundation
	ApexDesc.physXSDK				= GPhysXSDK;	// Pointer to the PhysXSDK
	ApexDesc.cooking				= GPhysXCooking;	// Pointer to the cooking library
	ApexDesc.renderResourceManager	= &GApexNullRenderResourceManager;	// We will not be using the APEX rendering API, so just use a dummy render resource manager
	ApexDesc.resourceCallback		= &GApexResourceCallback;	// The resource callback is how APEX asks the application to find assets when it needs them

#if PLATFORM_MAC
	FString DylibFolder = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/PhysX/");
	ANSICHAR* DLLLoadPath = (ANSICHAR*)FMemory::Malloc(DylibFolder.Len() + 1);
	FCStringAnsi::Strcpy(DLLLoadPath, DylibFolder.Len() + 1, TCHAR_TO_UTF8(*DylibFolder));
	ApexDesc.dllLoadPath = DLLLoadPath;
#endif

	// Create the APEX SDK
	apex::ApexCreateError ErrorCode;
	GApexSDK = apex::CreateApexSDK(ApexDesc, &ErrorCode);
	check(ErrorCode == APEX_CE_NO_ERROR);
	check(GApexSDK);

#if PLATFORM_MAC
	FMemory::Free(DLLLoadPath);
#endif

#if UE_BUILD_SHIPPING
	GApexSDK->setEnableApexStats(false);
#endif




#if APEX_STATICALLY_LINKED
	// We need to instantiate the module if we have statically linked them
	// Otherwise all createModule functions will fail
	instantiateModuleDestructible();

#if WITH_APEX_CLOTHING
	instantiateModuleClothing();
#endif

#if WITH_APEX_LEGACY
	instantiateModuleLegacy();
#endif
#endif

	// 1 legacy module for all in APEX 1.3
	// Load the only 1 legacy module
#if WITH_APEX_LEGACY
	GApexModuleLegacy = GApexSDK->createModule("Legacy");
	check(GApexModuleLegacy);
#endif // WITH_APEX_LEGACY

	// Load APEX Destruction module
	GApexModuleDestructible = static_cast<apex::ModuleDestructible*>(GApexSDK->createModule("Destructible"));
	check(GApexModuleDestructible);

	// Set Destructible module parameters
	NvParameterized::Interface* ModuleParams = GApexModuleDestructible->getDefaultModuleDesc();
	// ModuleParams contains the default module descriptor, which may be modified here before calling the module init function
	GApexModuleDestructible->init(*ModuleParams);
	// Set chunk report for fracture effect callbacks
	GApexModuleDestructible->setChunkReport(&GApexChunkReport);

	
	GApexModuleDestructible->setMaxDynamicChunkIslandCount((physx::PxU32)FMath::Max(CVarAPEXMaxDestructibleDynamicChunkIslandCount.GetValueOnGameThread(), 0));
	GApexModuleDestructible->setMaxChunkCount((physx::PxU32)FMath::Max(CVarAPEXMaxDestructibleDynamicChunkCount.GetValueOnGameThread(), 0));
	GApexModuleDestructible->setSortByBenefit(CVarAPEXSortDynamicChunksByBenefit.GetValueOnGameThread() != 0);

	GApexModuleDestructible->scheduleChunkStateEventCallback(apex::DestructibleCallbackSchedule::FetchResults);

	// APEX 1.3 to preserve 1.2 behavior
	GApexModuleDestructible->setUseLegacyDamageRadiusSpread(true); 
	GApexModuleDestructible->setUseLegacyChunkBoundsTesting(true);

#if WITH_APEX_CLOTHING
	// Load APEX Clothing module
	GApexModuleClothing = static_cast<apex::ModuleClothing*>(GApexSDK->createModule("Clothing"));
	check(GApexModuleClothing);
	// Set Clothing module parameters
	ModuleParams = GApexModuleClothing->getDefaultModuleDesc();

	// Can be tuned for switching between more memory and more spikes.
	NvParameterized::setParamU32(*ModuleParams, "maxUnusedPhysXResources", 5);

	// If true, let fetch results tasks run longer than the fetchResults call. 
	// Setting to true could not ensure same finish timing with Physx simulation phase
	NvParameterized::setParamBool(*ModuleParams, "asyncFetchResults", false);

	// ModuleParams contains the default module descriptor, which may be modified here before calling the module init function
	GApexModuleClothing->init(*ModuleParams);
#endif	//WITH_APEX_CLOTHING

#endif // #if WITH_APEX

#endif // WITH_PHYSX
}

void TermGamePhys()
{
#if WITH_BOX2D
	FPhysicsIntegration2D::ShutdownPhysics();
#endif

#if WITH_PHYSX
	FPhysxSharedData::Terminate();

	// Do nothing if they were never initialized
	if(GPhysXFoundation == NULL)
	{
		return;
	}

	if (GPhysCommandHandler != NULL)
	{
		GPhysCommandHandler->Flush();	//finish off any remaining commands
		FCoreUObjectDelegates::PreGarbageCollect.Remove(GPreGarbageCollectDelegateHandle);
		delete GPhysCommandHandler;
		GPhysCommandHandler = NULL;
	}

#if WITH_APEX
#if WITH_APEX_LEGACY
	if(GApexModuleLegacy != NULL)
	{
		GApexModuleLegacy->release();
		GApexModuleLegacy = NULL;
	}
#endif // WITH_APEX_LEGACY
	if(GApexSDK != NULL)
	{
		GApexSDK->release(); 
		GApexSDK = NULL;
	}
#endif	// #if WITH_APEX

	//Remove all scenes still registered
	if (int32 NumScenes = GPhysXSDK->getNbScenes())
	{
		TArray<PxScene*> PScenes;
		PScenes.AddUninitialized(NumScenes);
		GPhysXSDK->getScenes(PScenes.GetData(), sizeof(PxScene*)* NumScenes);

		for (PxScene* PScene : PScenes)
		{
			if (PScene)
			{
				PScene->release();
			}
		}
	}



#if WITH_PHYSICS_COOKING || WITH_RUNTIME_PHYSICS_COOKING
	if(GPhysXCooking != NULL)
	{
		GPhysXCooking->release(); 
		GPhysXCooking = NULL;
	}
#endif

	PxCloseExtensions();
	PxCloseVehicleSDK();

	if(GPhysXSDK != NULL)
	{
		GPhysXSDK->release();
		GPhysXSDK = NULL;
	}

	// @todo delete FPhysXAllocator
	// @todo delete FPhysXOutputStream

	UnloadPhysXModules();
#endif
}

/** 
*	Perform any cleanup of physics engine resources. 
*	This is deferred because when closing down the game, you want to make sure you are not destroying a mesh after the physics SDK has been shut down.
*/
void DeferredPhysResourceCleanup()
{
#if WITH_PHYSX

	// Release all tri meshes and reset array
	for(int32 MeshIdx=0; MeshIdx<GPhysXPendingKillTriMesh.Num(); MeshIdx++)
	{
		PxTriangleMesh* PTriMesh = GPhysXPendingKillTriMesh[MeshIdx];

		// Check this as it shouldn't be null, but then gate on it so we can
		// avoid a crash if we end up in this state in shipping
		check(PTriMesh);
		if(PTriMesh)
		{
			PTriMesh->release();

			if(GPhysXPendingKillTriMesh.IsValidIndex(MeshIdx))
			{
				GPhysXPendingKillTriMesh[MeshIdx] = NULL;
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("DeferredPhysResourceCleanup found invalid index into GPhysXPendingKillTriMesh, another thread may have modified the array."), MeshIdx);
			}
		}
		else
		{
			UE_LOG(LogPhysics, Warning, TEXT("DeferredPhysResourceCleanup found null PxTriangleMesh in pending kill array, another thread may have modified the array."), MeshIdx);
		}
	}
	GPhysXPendingKillTriMesh.Reset();

	// Release all convex meshes and reset array
	for(int32 MeshIdx=0; MeshIdx<GPhysXPendingKillConvex.Num(); MeshIdx++)
	{
		PxConvexMesh* PConvexMesh = GPhysXPendingKillConvex[MeshIdx];

		// Check this as it shouldn't be null, but then gate on it so we can
		// avoid a crash if we end up in this state in shipping
		check(PConvexMesh);
		if(PConvexMesh)
		{
			PConvexMesh->release();

			if(GPhysXPendingKillConvex.IsValidIndex(MeshIdx))
			{
				GPhysXPendingKillConvex[MeshIdx] = NULL;
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("DeferredPhysResourceCleanup found invalid index into GPhysXPendingKillConvex (%d), another thread may have modified the array."), MeshIdx);
			}
		}
		else
		{
			UE_LOG(LogPhysics, Warning, TEXT("DeferredPhysResourceCleanup found null PxConvexMesh in pending kill array (at %d), another thread may have modified the array."), MeshIdx);
		}
	}
	GPhysXPendingKillConvex.Reset();

	// Release all heightfields and reset array
	for(int32 HfIdx=0; HfIdx<GPhysXPendingKillHeightfield.Num(); HfIdx++)
	{
		PxHeightField* PHeightfield = GPhysXPendingKillHeightfield[HfIdx];

		// Check this as it shouldn't be null, but then gate on it so we can
		// avoid a crash if we end up in this state in shipping
		check(PHeightfield);
		if(PHeightfield)
		{
			PHeightfield->release();

			if(GPhysXPendingKillHeightfield.IsValidIndex(HfIdx))
			{
				GPhysXPendingKillHeightfield[HfIdx] = NULL;
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("DeferredPhysResourceCleanup found invalid index into GPhysXPendingKillHeightfield (%d), another thread may have modified the array."), HfIdx);
			}
		}
		else
		{
			UE_LOG(LogPhysics, Warning, TEXT("DeferredPhysResourceCleanup found null PxHeightField in pending kill array (at %d), another thread may have modified the array."), HfIdx);
		}
	}
	GPhysXPendingKillHeightfield.Reset();

	// Release all materials and reset array
	for(int32 MeshIdx=0; MeshIdx<GPhysXPendingKillMaterial.Num(); MeshIdx++)
	{
		PxMaterial* PMaterial = GPhysXPendingKillMaterial[MeshIdx];

		// Check this as it shouldn't be null, but then gate on it so we can
		// avoid a crash if we end up in this state in shipping
		check(PMaterial);
		if(PMaterial)
		{
			PMaterial->release();
			if(GPhysXPendingKillMaterial.IsValidIndex(MeshIdx))
			{
				GPhysXPendingKillMaterial[MeshIdx] = NULL;
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("DeferredPhysResourceCleanup found invalid index into GPhysXPendingKillMaterial(%d), another thread may have modified the array."), MeshIdx);
			}
		}
		else
		{
			UE_LOG(LogPhysics, Warning, TEXT("DeferredPhysResourceCleanup found null PxMaterial in pending kill array (at %d), another thread may have modified the array."), MeshIdx);
		}
	}
	GPhysXPendingKillMaterial.Reset();
#endif
}

