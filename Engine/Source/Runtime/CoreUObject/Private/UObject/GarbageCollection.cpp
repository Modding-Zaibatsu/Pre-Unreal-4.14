// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjGC.cpp: Unreal object garbage collection code.
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "TaskGraphInterfaces.h"
#include "IConsoleManager.h"
#include "LinkerPlaceholderClass.h"
#include "UObject/GCScopeLock.h"
#include "HAL/ExceptionHandling.h"

/*-----------------------------------------------------------------------------
   Garbage collection.
-----------------------------------------------------------------------------*/

#define PERF_DETAILED_PER_CLASS_GC_STATS				(LOOKING_FOR_PERF_ISSUES || 0) 

// FastReferenceCollector uses PERF_DETAILED_PER_CLASS_GC_STATS
#include "UObject/FastReferenceCollector.h"

DEFINE_LOG_CATEGORY(LogGarbage);

// UE_BUILD_SHIPPING has GShouldVerifyGCAssumptions=false by default
#define VERIFY_DISREGARD_GC_ASSUMPTIONS			!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#define	ENABLE_GC_DEBUG_OUTPUT					1

/** Object count during last mark phase																				*/
int32		GObjectCountDuringLastMarkPhase			= 0;
/** Count of objects purged since last mark phase																	*/
int32		GPurgedObjectCountSinceLastMarkPhase	= 0;
/** Whether incremental object purge is in progress										*/
bool GObjIncrementalPurgeIsInProgress = false;
/** Whether GC is currently routing BeginDestroy to objects										*/
bool GObjUnhashUnreachableIsInProgress = false;
/** Whether FinishDestroy has already been routed to all unreachable objects. */
static bool GObjFinishDestroyHasBeenRoutedToAllObjects	= false;
/** 
 * Array that we'll fill with indices to objects that are still pending destruction after
 * the first GC sweep (because they weren't ready to be destroyed yet.) 
 */
static TArray<UObject *> GGCObjectsPendingDestruction;
/** Number of objects actually still pending destruction */
static int32 GGCObjectsPendingDestructionCount = 0;
/** Whether we need to purge objects or not.											*/
static bool GObjPurgeIsRequired = false;
/** Current object index for incremental purge.											*/
static FRawObjectIterator GObjCurrentPurgeObjectIndex;
/** Current object index for incremental purge.											*/
static bool GObjCurrentPurgeObjectIndexNeedsReset = true;
static bool GObjCurrentPurgeObjectIndexResetPastPermanent = false;

/** Currently running a parallel reachability test.											*/
static volatile bool GIsRunningParallelReachability = false;

/** Whether we are currently purging an object in the GC purge pass. */
static bool GIsPurgingObject = false;

/** Helpful constant for determining how many token slots we need to store a pointer **/
static const uint32 GNumTokensPerPointer = sizeof(void*) / sizeof(uint32);

FThreadSafeBool& FGCScopeLock::GetGarbageCollectingFlag()
{
	static FThreadSafeBool IsGarbageCollecting(false);
	return IsGarbageCollecting;
}

FGCCSyncObject GGarbageCollectionGuardCritical;

FGCScopeGuard::FGCScopeGuard()
{
	GGarbageCollectionGuardCritical.LockAsync();
}
FGCScopeGuard::~FGCScopeGuard()
{
	GGarbageCollectionGuardCritical.UnlockAsync();
}

bool IsGarbageCollecting()
{
	return FGCScopeLock::GetGarbageCollectingFlag();
}

bool IsGarbageCollectionLocked()
{
	return GGarbageCollectionGuardCritical.IsAsyncLocked();
}

/**
 * Pool for reducing GC allocations
 */
class FGCArrayPool
{
public:

	/**
	* Gets the singleton instance of the FObjectArrayPool
	* @return Pool singleton.
	*/
	FORCEINLINE static FGCArrayPool& Get()
	{
		static FGCArrayPool Singleton;
		return Singleton;
	}

	/**
	* Gets an event from the pool or creates one if necessary.
	*
	* @return The array.
	* @see ReturnToPool
	*/
	FORCEINLINE TArray<UObject*>* GetArrayFromPool()
	{
		TArray<UObject*>* Result = Pool.Pop();
		if (!Result)
		{
			Result = new TArray<UObject*>();
		}
		check(Result);
#if UE_BUILD_DEBUG
		NumberOfUsedArrays.Increment();
#endif // UE_BUILD_DEBUG
		return Result;
	}

	/**
	* Returns an array to the pool.
	*
	* @param Array The array to return.
	* @see GetArrayFromPool
	*/
	FORCEINLINE void ReturnToPool(TArray<UObject*>* Array)
	{
#if UE_BUILD_DEBUG
		const int32 CheckUsedArrays = NumberOfUsedArrays.Decrement();
		checkSlow(CheckUsedArrays >= 0);
#endif // UE_BUILD_DEBUG
		check(Array);
		Array->Reset();
		Pool.Push(Array);
	}

	/** Performs memory cleanup */
	void Cleanup()
	{
#if UE_BUILD_DEBUG
		const int32 CheckUsedArrays = NumberOfUsedArrays.GetValue();
		checkSlow(CheckUsedArrays == 0);
#endif // UE_BUILD_DEBUG

		uint32 FreedMemory = 0;
		TArray< TArray<UObject*>* > AllArrays;
		Pool.PopAll(AllArrays);
		for (TArray<UObject*>* Array : AllArrays)
		{
			FreedMemory += Array->GetAllocatedSize();
			delete Array;
		}
		UE_LOG(LogGarbage, Log, TEXT("Freed %ub from %d GC array pools."), FreedMemory, AllArrays.Num());
	}

#if UE_BUILD_DEBUG
	void CheckLeaks()
	{
		// This function is called after GC has finished so at this point there should be no
		// arrays used by GC and all should be returned to the pool
		const int32 LeakedGCPoolArrays = NumberOfUsedArrays.GetValue();
		checkSlow(LeakedGCPoolArrays == 0);
	}
#endif

private:

	/** Holds the collection of recycled arrays. */
	TLockFreePointerListLIFO< TArray<UObject*> > Pool;

#if UE_BUILD_DEBUG
	/** Number of arrays currently acquired from the pool by GC */
	FThreadSafeCounter NumberOfUsedArrays;
#endif // UE_BUILD_DEBUG
};

void CleanupClusterArrayPools();
/** Called on shutdown to free GC memory */
void CleanupGCArrayPools()
{
	FGCArrayPool::Get().Cleanup();
	CleanupClusterArrayPools();
}

/**
 * If set and VERIFY_DISREGARD_GC_ASSUMPTIONS is true, we verify GC assumptions about "Disregard For GC" objects. We also
 * verify that no unreachable actors/ components are referenced if VERIFY_NO_UNREACHABLE_OBJECTS_ARE_REFERENCED
 * is true.
 */
COREUOBJECT_API bool	GShouldVerifyGCAssumptions				= !(UE_BUILD_SHIPPING != 0 && WITH_EDITOR != 0);

// Minimum number of objects to spawn a GC sub-task for
static int32 GMinDesiredObjectsPerSubTask = 128;
static FAutoConsoleVariableRef CVarMinDesiredObjectsPerSubTask(
	TEXT("gc.MinDesiredObjectsPerSubTask"),
	GMinDesiredObjectsPerSubTask,
	TEXT("Minimum number of objects to spawn a GC sub-task for."),
	ECVF_Default
	);

#if PERF_DETAILED_PER_CLASS_GC_STATS
/** Map from a UClass' FName to the number of objects that were purged during the last purge phase of this class.	*/
static TMap<const FName,uint32> GClassToPurgeCountMap;
/** Map from a UClass' FName to the number of "Disregard For GC" object references followed for all instances.		*/
static TMap<const FName,uint32> GClassToDisregardedObjectRefsMap;
/** Map from a UClass' FName to the number of regular object references followed for all instances.					*/
static TMap<const FName,uint32> GClassToRegularObjectRefsMap;
/** Map from a UClass' FName to the number of cycles spent with GC.													*/
static TMap<const FName,uint32> GClassToCyclesMap;

/** Number of disregarded object refs for current object.															*/
static uint32 GCurrentObjectDisregardedObjectRefs;
/** Number of regulard object refs for current object.																*/
static uint32 GCurrentObjectRegularObjectRefs;

/**
 * Helper structure used for sorting class to count map.
 */
struct FClassCountInfo
{
	FName	ClassName;
	uint32	InstanceCount;
};

/**
 * Helper function to log the various class to count info maps.
 *
 * @param	LogText				Text to emit between number and class 
 * @param	ClassToCountMap		TMap from a class' FName to "count"
 * @param	NumItemsToList		Number of items to log
 * @param	TotalCount			Total count, if 0 will be calculated
 */
static void LogClassCountInfo( const TCHAR* LogText, TMap<const FName,uint32>& ClassToCountMap, int32 NumItemsToLog, uint32 TotalCount )
{
	// Array of class name and counts.
	TArray<FClassCountInfo> ClassCountArray;
	ClassCountArray.Empty( ClassToCountMap.Num() );

	// Figure out whether we need to calculate the total count.
	bool bNeedToCalculateCount = false;
	if( TotalCount == 0 )
	{
		bNeedToCalculateCount = true;
	}
	// Copy TMap to TArray for sorting purposes (and to calculate count if needed).
	for( TMap<const FName,uint32>::TIterator It(ClassToCountMap); It; ++It )
	{
		FClassCountInfo ClassCountInfo;
		ClassCountInfo.ClassName		= It.Key();
		ClassCountInfo.InstanceCount	= It.Value();
		ClassCountArray.Add( ClassCountInfo );
		if( bNeedToCalculateCount )
		{
			TotalCount += ClassCountInfo.InstanceCount;
		}
	}
	// Sort array by instance count.
	struct FCompareFClassCountInfo
	{
		FORCEINLINE bool operator()( const FClassCountInfo& A, const FClassCountInfo& B ) const
		{
			return B.InstanceCount < A.InstanceCount;
		}
	};
	ClassCountArray.Sort( FCompareFClassCountInfo() );

	// Log top NumItemsToLog class counts
	for( int32 Index=0; Index<FMath::Min(NumItemsToLog,ClassCountArray.Num()); Index++ )
	{
		const FClassCountInfo& ClassCountInfo = ClassCountArray[Index];
		const float Percent = 100.f * ClassCountInfo.InstanceCount / TotalCount;
		const FString PercentString = (TotalCount > 0) ? FString::Printf(TEXT("%6.2f%%"), Percent) : FString(TEXT("  N/A  "));
		UE_LOG(LogGarbage, Log, TEXT("%5d [%s] %s Class %s"), ClassCountInfo.InstanceCount, *PercentString, LogText, *ClassCountInfo.ClassName.ToString() ); 
	}

	// Empty the map for the next run.
	ClassToCountMap.Empty();
};
#endif

/**
* Handles UObject references found by TFastReferenceCollector
*/
class FGCReferenceProcessor
{
public:

	FGCReferenceProcessor()
	{
	}

	FORCEINLINE int32 GetMinDesiredObjectsPerSubTask() const
	{
		return GMinDesiredObjectsPerSubTask;
	}

	FORCEINLINE volatile bool IsRunningMultithreaded() const
	{
		return GIsRunningParallelReachability;
	}

	FORCEINLINE void SetIsRunningMultithreaded(bool bIsParallel)
	{
		GIsRunningParallelReachability = bIsParallel;
	}

	void UpdateDetailedStats(UObject* CurrentObject, uint32 DeltaCycles)
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		// Keep track of how many refs we encountered for the object's class.
		const FName& ClassName = CurrentObject->GetClass()->GetFName();
		// Refs to objects that reside in permanent object pool.
		uint32 ClassDisregardedObjRefs = GClassToDisregardedObjectRefsMap.FindRef(ClassName);
		GClassToDisregardedObjectRefsMap.Add(ClassName, ClassDisregardedObjRefs + GCurrentObjectDisregardedObjectRefs);
		// Refs to regular objects.
		uint32 ClassRegularObjRefs = GClassToRegularObjectRefsMap.FindRef(ClassName);
		GClassToRegularObjectRefsMap.Add(ClassName, ClassRegularObjRefs + GCurrentObjectRegularObjectRefs);
		// Track per class cycle count spent in GC.
		uint32 ClassCycles = GClassToCyclesMap.FindRef(ClassName);
		GClassToCyclesMap.Add(ClassName, ClassCycles + DeltaCycles);
		// Reset current counts.
		GCurrentObjectDisregardedObjectRefs = 0;
		GCurrentObjectRegularObjectRefs = 0;
#endif
	}

	void LogDetailedStatsSummary()
	{
#if PERF_DETAILED_PER_CLASS_GC_STATS
		LogClassCountInfo(TEXT("references to regular objects from"), GClassToRegularObjectRefsMap, 20, 0);
		LogClassCountInfo(TEXT("references to permanent objects from"), GClassToDisregardedObjectRefsMap, 20, 0);
		LogClassCountInfo(TEXT("cycles for GC"), GClassToCyclesMap, 20, 0);
#endif
	}

	/** Marks all objects that can't be directly in a cluster but are referenced by it as reachable */
	template <bool bParallel>
	FORCEINLINE void MarkClusterMutableObjectsAsReachable(FUObjectCluster* Cluster, TArray<UObject*>& ObjectsToSerialize)
	{
		for (int32 ReferencedMutableObjectIndex : Cluster->MutableObjects)
		{
			FUObjectItem* ReferencedMutableObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ReferencedMutableObjectIndex);
			if (bParallel)
			{
				if (ReferencedMutableObjectItem->IsUnreachable() && ReferencedMutableObjectItem->ThisThreadAtomicallyClearedRFUnreachable())
				{
					ReferencedMutableObjectItem->ThisThreadAtomicallyClearedFlag(EInternalObjectFlags::NoStrongReference);
					ObjectsToSerialize.Add(static_cast<UObject*>(ReferencedMutableObjectItem->Object));
				}
			}
			else if (ReferencedMutableObjectItem->IsUnreachable())
			{
				ReferencedMutableObjectItem->ClearFlags(EInternalObjectFlags::NoStrongReference | EInternalObjectFlags::Unreachable);
				ObjectsToSerialize.Add(static_cast<UObject*>(ReferencedMutableObjectItem->Object));
			}
		}
	}

	/** Marks all clusters referenced by another cluster as rechable */
	template <bool bParallel>
	FORCEINLINE void MarkReferencedClustersAsReachable(int32 ClusterRootIndex, TArray<UObject*>& ObjectsToSerialize)
	{
		FUObjectCluster* Cluster = GUObjectClusters.FindChecked(ClusterRootIndex);
		// Also mark all referenced objects from outside of the cluster as reachable
		MarkClusterMutableObjectsAsReachable<bParallel>(Cluster, ObjectsToSerialize);
		for (int32 ReferncedClusterIndex : Cluster->ReferencedClusters)
		{
			FUObjectItem* ReferencedClusterRootObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ReferncedClusterIndex);
			// This condition should get collapsed by the compiler based on the template argument
			if (bParallel)
			{
				ReferencedClusterRootObjectItem->ThisThreadAtomicallyClearedFlag(EInternalObjectFlags::NoStrongReference | EInternalObjectFlags::Unreachable);
			}
			else
			{
				ReferencedClusterRootObjectItem->ClearFlags(EInternalObjectFlags::NoStrongReference | EInternalObjectFlags::Unreachable);
			}
			FUObjectCluster* ReferencedCluster = GUObjectClusters.FindChecked(ReferncedClusterIndex);
			MarkClusterMutableObjectsAsReachable<bParallel>(ReferencedCluster, ObjectsToSerialize);
		}		
	}

	/**
	 * Handles object reference, potentially NULL'ing
	 *
	 * @param Object						Object pointer passed by reference
	 * @param ReferencingObject UObject which owns the reference (can be NULL)
	 * @param bAllowReferenceElimination	Whether to allow NULL'ing the reference if RF_PendingKill is set
	*/
	FORCEINLINE void HandleObjectReference(TArray<UObject*>& ObjectsToSerialize, const UObject * const ReferencingObject, UObject*& Object, const bool bAllowReferenceElimination, const bool bStrongReference = true)
	{
		// Disregard NULL objects and perform very fast check to see whether object is part of permanent
		// object pool and should therefore be disregarded. The check doesn't touch the object and is
		// cache friendly as it's just a pointer compare against to globals.
		const bool IsInPermanentPool = GUObjectAllocator.ResidesInPermanentPool(Object);

#if PERF_DETAILED_PER_CLASS_GC_STATS
		if (IsInPermanentPool)
		{
			GCurrentObjectDisregardedObjectRefs++;
		}
#endif
		if (Object == nullptr || IsInPermanentPool)
		{
			return;
		}

		FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(Object);
		// Remove references to pending kill objects if we're allowed to do so.
		if (ObjectItem->IsPendingKill() && bAllowReferenceElimination)
		{
			// Null out reference.
			Object = NULL;
		}
		// Add encountered object reference to list of to be serialized objects if it hasn't already been added.
		else if (ObjectItem->IsUnreachable())
		{
			if (GIsRunningParallelReachability)
			{
				// Mark it as reachable.
				if (ObjectItem->ThisThreadAtomicallyClearedRFUnreachable())
				{
					// Objects that are part of a GC cluster should never have the unreachable flag set!
					checkSlow(ObjectItem->GetOwnerIndex() == 0);

					if (!ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
					{
						// Add it to the list of objects to serialize.
						ObjectsToSerialize.Add(Object);
					}
					else
					{
						// This is a cluster root reference so mark all referenced clusters as reachable
						const int32 ObjectIndex = GUObjectArray.ObjectToIndex(Object);
						MarkReferencedClustersAsReachable<true>(ObjectIndex, ObjectsToSerialize);
					}
				}
			}
			else
			{
#if ENABLE_GC_DEBUG_OUTPUT
				// this message is to help track down culprits behind "Object in PIE world still referenced" errors
				if (GIsEditor && !GIsPlayInEditorWorld && ReferencingObject != NULL && !ReferencingObject->RootPackageHasAnyFlags(PKG_PlayInEditor) && Object->RootPackageHasAnyFlags(PKG_PlayInEditor))
				{
					UE_LOG(LogGarbage, Warning, TEXT("GC detected illegal reference to PIE object from content [possibly via [todo]]:"));
					UE_LOG(LogGarbage, Warning, TEXT("      PIE object: %s"), *Object->GetFullName());
					UE_LOG(LogGarbage, Warning, TEXT("  NON-PIE object: %s"), *ReferencingObject->GetFullName());
				}
#endif

				// Mark it as reachable.
				ObjectItem->ClearUnreachable();

				// Objects that are part of a GC cluster should never have the unreachable flag set!
				checkSlow(ObjectItem->GetOwnerIndex() == 0);

				if (!ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
				{
					// Add it to the list of objects to serialize.
					ObjectsToSerialize.Add(Object);
				}
				else
				{
					// This is a cluster root reference so mark all referenced clusters as reachable
					const int32 ObjectIndex = GUObjectArray.ObjectToIndex(Object);
					MarkReferencedClustersAsReachable<false>(ObjectIndex, ObjectsToSerialize);
				}
			}
		}
		else if (ObjectItem->GetOwnerIndex() && !ObjectItem->HasAnyFlags(EInternalObjectFlags::ReachableInCluster))
		{
			ObjectItem->SetFlags(EInternalObjectFlags::ReachableInCluster);
			// Make sure cluster root object is reachable too
			const int32 OwnerIndex = ObjectItem->GetOwnerIndex();
			FUObjectItem* RootObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(OwnerIndex);
			checkSlow(RootObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot));
			if (GIsRunningParallelReachability)
			{
				if (RootObjectItem->ThisThreadAtomicallyClearedRFUnreachable())
				{
					RootObjectItem->ThisThreadAtomicallyClearedFlag(EInternalObjectFlags::NoStrongReference);
					// Make sure all referenced clusters are marked as reachable too
					MarkReferencedClustersAsReachable<true>(OwnerIndex, ObjectsToSerialize);
				}
			}
			else if (RootObjectItem->IsUnreachable())
			{
				RootObjectItem->ClearFlags(EInternalObjectFlags::Unreachable | EInternalObjectFlags::NoStrongReference);
				// Make sure all referenced clusters are marked as reachable too
				MarkReferencedClustersAsReachable<false>(OwnerIndex, ObjectsToSerialize);
			}
		}

		// The second condition seems to improve perf for multithreaded GC
		if (bStrongReference && ObjectItem->HasAnyFlags(EInternalObjectFlags::NoStrongReference))
		{
			ObjectItem->ThisThreadAtomicallyClearedFlag(EInternalObjectFlags::NoStrongReference);
		}
#if PERF_DETAILED_PER_CLASS_GC_STATS
		GCurrentObjectRegularObjectRefs++;
#endif
	}

	/**
	* Handles UObject reference from the token stream.
	*
	* @param ObjectsToSerialize An array of remaining objects to serialize.
	* @param ReferencingObject Object referencing the object to process.
	* @param TokenIndex Index to the token stream where the reference was found.
	* @param bAllowReferenceElimination True if reference elimination is allowed.
	*/
	FORCEINLINE void HandleTokenStreamObjectReference(TArray<UObject*>& ObjectsToSerialize, UObject* ReferencingObject, UObject*& Object, const int32 TokenIndex, bool bAllowReferenceElimination)
	{
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		if (Object)
		{
			if (
#if DO_POINTER_CHECKS_ON_GC
				!IsPossiblyAllocatedUObjectPointer(Object) ||
#endif
				!Object->IsValidLowLevelFast())
			{
				FString TokenDebugInfo;
				if (UClass *Class = (ReferencingObject ? ReferencingObject->GetClass() : nullptr))
				{
					auto& TokenInfo = Class->DebugTokenMap.GetTokenInfo(TokenIndex);
					TokenDebugInfo = FString::Printf(TEXT("ReferencingObjectClass: %s, Property Name: %s, Offset: %d"),
						*Class->GetFullName(), *TokenInfo.Name.GetPlainNameString(), TokenInfo.Offset);
				}
				else
				{
					// This means this objects is most likely being referenced by AddReferencedObjects
					TokenDebugInfo = TEXT("Native Reference");
				}

				UE_LOG(LogGarbage, Fatal, TEXT("Invalid object in GC: 0x%016llx, ReferencingObject: %s, %s, TokenIndex: %d"),
					(int64)(PTRINT)Object,
					ReferencingObject ? *ReferencingObject->GetFullName() : TEXT("NULL"),
					*TokenDebugInfo, TokenIndex);
			}
		}
#endif
		HandleObjectReference(ObjectsToSerialize, ReferencingObject, Object, bAllowReferenceElimination);
	}
};

/**
* Specialized FReferenceCollector that uses FGCReferenceProcessor to mark objects as reachable.
*/
class FGCCollector : public FReferenceCollector
{
	FGCReferenceProcessor& ReferenceProcessor;
	TArray<UObject*>& ObjectArray;
	bool bAllowEliminatingReferences;
	bool bShouldHandleAsWeakRef;

public:

	FGCCollector(FGCReferenceProcessor& InProcessor, TArray<UObject*>& InObjectArray)
		: ReferenceProcessor(InProcessor)
		, ObjectArray(InObjectArray)
		, bAllowEliminatingReferences(true)
		, bShouldHandleAsWeakRef(false)
	{
	}

	virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const UProperty* ReferencingProperty) override
	{
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
		if (Object && !Object->IsValidLowLevelFast())
		{
			UE_LOG(LogGarbage, Fatal, TEXT("Invalid object in GC: 0x%016llx, ReferencingObject: %s, ReferencingProperty: %s"), 
				(int64)(PTRINT)Object, 
				ReferencingObject ? *ReferencingObject->GetFullName() : TEXT("NULL"),
				ReferencingProperty ? *ReferencingProperty->GetFullName() : TEXT("NULL"));
		}
#endif
		ReferenceProcessor.HandleObjectReference(ObjectArray, const_cast<UObject*>(ReferencingObject), Object, bAllowEliminatingReferences, !bShouldHandleAsWeakRef);
	}
	virtual void HandleObjectReferences(UObject** InObjects, const int32 ObjectNum, const UObject* InReferencingObject, const UProperty* InReferencingProperty) override
	{
		for (int32 ObjectIndex = 0; ObjectIndex < ObjectNum; ++ObjectIndex)
		{
			UObject*& Object = InObjects[ObjectIndex];
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
			if (Object && !Object->IsValidLowLevelFast())
			{
				UE_LOG(LogGarbage, Fatal, TEXT("Invalid object in GC: 0x%016llx, ReferencingObject: %s, ReferencingProperty: %s"),
					(int64)(PTRINT)Object,
					InReferencingObject ? *InReferencingObject->GetFullName() : TEXT("NULL"),
					InReferencingProperty ? *InReferencingProperty->GetFullName() : TEXT("NULL"));
			}
#endif
			ReferenceProcessor.HandleObjectReference(ObjectArray, const_cast<UObject*>(InReferencingObject), Object, bAllowEliminatingReferences, !bShouldHandleAsWeakRef);
		}
	}

	virtual bool IsIgnoringArchetypeRef() const override
	{
		return false;
	}
	virtual bool IsIgnoringTransient() const override
	{
		return false;
	}
	virtual void AllowEliminatingReferences( bool bAllow ) override
	{
		bAllowEliminatingReferences = bAllow;
	}

	virtual void SetShouldHandleAsWeakRef(bool bWeakRef) override
	{
		bShouldHandleAsWeakRef = bWeakRef;
	}
};


/*----------------------------------------------------------------------------
	FReferenceFinder.
----------------------------------------------------------------------------*/
FReferenceFinder::FReferenceFinder( TArray<UObject*>& InObjectArray, UObject* InOuter, bool bInRequireDirectOuter, bool bInShouldIgnoreArchetype, bool bInSerializeRecursively, bool bInShouldIgnoreTransient )
	:	ObjectArray( InObjectArray )
	,	LimitOuter( InOuter )
	, SerializedProperty(nullptr)
	,	bRequireDirectOuter( bInRequireDirectOuter )
	, bShouldIgnoreArchetype( bInShouldIgnoreArchetype )
	, bSerializeRecursively( false )
	, bShouldIgnoreTransient( bInShouldIgnoreTransient )
{
	bSerializeRecursively = bInSerializeRecursively && LimitOuter != NULL;
	if (InOuter)
	{
		// If the outer is specified, try to set the SerializedProperty based on its linker.
		auto OuterLinker = InOuter->GetLinker();
		if (OuterLinker)
		{
			SerializedProperty = OuterLinker->GetSerializedProperty();
		}
	}
}

void FReferenceFinder::FindReferences(UObject* Object, UObject* InReferencingObject, UProperty* InReferencingProperty)
{
	check(Object != NULL);

	if (!Object->GetClass()->IsChildOf(UClass::StaticClass()))
	{
		FSimpleObjectReferenceCollectorArchive CollectorArchive(Object, *this);
		CollectorArchive.SetSerializedProperty(SerializedProperty);
		Object->SerializeScriptProperties(CollectorArchive);
	}
	Object->CallAddReferencedObjects(*this);
}

void FReferenceFinder::HandleObjectReference( UObject*& InObject, const UObject* InReferencingObject /*= NULL*/, const UProperty* InReferencingProperty /*= NULL*/ )
{
	// Avoid duplicate entries.
	if ( InObject != NULL )
	{		
		if ( LimitOuter == NULL || (InObject->GetOuter() == LimitOuter || (!bRequireDirectOuter && InObject->IsIn(LimitOuter))) )
		{
			// Many places that use FReferenceFinder expect the object to not be const.
			UObject* Object = const_cast<UObject*>(InObject);
			// do not attempt to serialize objects that have already been 
			if ( ObjectArray.Contains( Object ) == false )
			{
				check( Object->IsValidLowLevel() );
				ObjectArray.Add( Object );
			}

			// check this object for any potential object references
			if ( bSerializeRecursively == true && !SerializedObjects.Find(Object) )
			{
				SerializedObjects.Add(Object);
				FindReferences(Object, const_cast<UObject*>(InReferencingObject), const_cast<UProperty*>(InReferencingProperty));
			}
		}
	}
}

/**
 * Implementation of parallel realtime garbage collector using recursive subdivision
 *
 * The approach is to create an array of uint32 tokens for each class that describe object references. This is done for 
 * script exposed classes by traversing the properties and additionally via manual function calls to emit tokens for
 * native only classes in the construction singleton IMPLEMENT_INTRINSIC_CLASS. 
 * A third alternative is a AddReferencedObjects callback per object which 
 * is used to deal with object references from types that aren't supported by the reflectable type system.
 * interface doesn't make sense to implement for.
 */
class FRealtimeGC
{
public:
	/** Default constructor, initializing all members. */
	FRealtimeGC()
	{}

	/** 
	 * Marks all objects that don't have KeepFlags and EInternalObjectFlags::GarbageCollectionKeepFlags as unreachable
	 * This function is a template to speed up the case where we don't need to assemble the token stream (saves about 6ms on PS4)
	 */
	void MarkObjectsAsUnreachable(TArray<UObject*>& ObjectsToSerialize, const EObjectFlags KeepFlags)
	{
		const EInternalObjectFlags FastKeepFlags = EInternalObjectFlags::GarbageCollectionKeepFlags;

		// Iterate over all objects. Note that we iterate over the UObjectArray and usually check only internal flags which
		// are part of the array so we don't suffer from cache misses as much as we would if we were to check ObjectFlags.
		for (FRawObjectIterator It(true); It; ++It)
		{
			FUObjectItem* ObjectItem = *It;
			checkSlow(ObjectItem);
			UObject* Object = (UObject*)ObjectItem->Object;

			// We can't collect garbage during an async load operation and by now all unreachable objects should've been purged.
			checkf(!ObjectItem->IsUnreachable(), TEXT("%s"), *Object->GetFullName());

			// Keep track of how many objects are around.
			GObjectCountDuringLastMarkPhase++;
			ObjectItem->ClearFlags(EInternalObjectFlags::ReachableInCluster);
			// Special case handling for objects that are part of the root set.
			if (ObjectItem->IsRootSet())
			{
				// IsValidLowLevel is extremely slow in this loop so only do it in debug
				checkSlow(Object->IsValidLowLevel());
				// We cannot use RF_PendingKill on objects that are part of the root set.
				checkCode(if (ObjectItem->IsPendingKill()) { UE_LOG(LogGarbage, Fatal, TEXT("Object %s is part of root set though has been marked RF_PendingKill!"), *Object->GetFullName()); });
				ObjectsToSerialize.Add(Object);
			}
			// Regular objects.
			else if (ObjectItem->GetOwnerIndex() == 0)
			{
				bool bMarkAsUnreachable = true;
				if (!ObjectItem->IsPendingKill())
				{
					// Internal flags are super fast to check
					if (ObjectItem->HasAnyFlags(FastKeepFlags))
					{
						bMarkAsUnreachable = false;
					}
					// If KeepFlags is non zero this is going to be very slow due to cache misses
					else if (KeepFlags != RF_NoFlags && Object->HasAnyFlags(KeepFlags))
					{
						bMarkAsUnreachable = false;
					}
				}

				// Mark objects as unreachable unless they have any of the passed in KeepFlags set and it's not marked for elimination..
				if (!bMarkAsUnreachable)
				{
					// IsValidLowLevel is extremely slow in this loop so only do it in debug
					checkSlow(Object->IsValidLowLevel());
					ObjectsToSerialize.Add(Object);
				}
				else
				{
					ObjectItem->SetFlags(EInternalObjectFlags::Unreachable | EInternalObjectFlags::NoStrongReference);
				}
			}
		}
	}

	/**
	 * Performs reachability analysis.
	 *
	 * @param KeepFlags		Objects with these flags will be kept regardless of being referenced or not
	 */
	void PerformReachabilityAnalysis(EObjectFlags KeepFlags, bool bForceSingleThreaded = false)
	{		
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FRealtimeGC::PerformReachabilityAnalysis"), STAT_FArchiveRealtimeGC_PerformReachabilityAnalysis, STATGROUP_GC);

		/** Growing array of objects that require serialization */
		TArray<UObject*>& ObjectsToSerialize = *FGCArrayPool::Get().GetArrayFromPool();

		// Reset object count.
		GObjectCountDuringLastMarkPhase = 0;

		// Presize array and add a bit of extra slack for prefetching.
		ObjectsToSerialize.Reset( GUObjectArray.GetObjectArrayNumMinusPermanent() + 3 );
		// Make sure GC referencer object is checked for references to other objects even if it resides in permanent object pool
		if (FPlatformProperties::RequiresCookedData() && FGCObject::GGCObjectReferencer && GUObjectArray.IsDisregardForGC(FGCObject::GGCObjectReferencer))
		{
			ObjectsToSerialize.Add(FGCObject::GGCObjectReferencer);
		}

		MarkObjectsAsUnreachable(ObjectsToSerialize, KeepFlags);

		{
			FGCReferenceProcessor ReferenceProcessor;
			TFastReferenceCollector<FGCReferenceProcessor, FGCCollector, FGCArrayPool> ReferenceCollector(ReferenceProcessor, FGCArrayPool::Get());
			ReferenceCollector.CollectReferences(ObjectsToSerialize, bForceSingleThreaded);
		}

		FGCArrayPool::Get().ReturnToPool(&ObjectsToSerialize);

#if UE_BUILD_DEBUG
		FGCArrayPool::Get().CheckLeaks();
#endif
	}
};

/**
 * Incrementally purge garbage by deleting all unreferenced objects after routing Destroy.
 *
 * Calling code needs to be EXTREMELY careful when and how to call this function as 
 * RF_Unreachable cannot change on any objects unless any pending purge has completed!
 *
 * @param	bUseTimeLimit	whether the time limit parameter should be used
 * @param	TimeLimit		soft time limit for this function call
 */
void IncrementalPurgeGarbage( bool bUseTimeLimit, float TimeLimit )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "IncrementalPurgeGarbage" ), STAT_IncrementalPurgeGarbage, STATGROUP_GC );

	if (GExitPurge)
	{
		GObjPurgeIsRequired = true;
		GUObjectArray.DisableDisregardForGC();
		GObjCurrentPurgeObjectIndexNeedsReset = true;
		GObjCurrentPurgeObjectIndexResetPastPermanent = false;
	}
	// Early out if there is nothing to do.
	if( !GObjPurgeIsRequired )
	{
		return;
	}

	// Set 'I'm garbage collecting' flag - might be checked inside UObject::Destroy etc.
	FGCScopeLock GCLock;

	// Incremental purge is now in progress.
	GObjIncrementalPurgeIsInProgress						= true;

	// Keep track of start time to enforce time limit unless bForceFullPurge is true;
	const double		StartTime							= FPlatformTime::Seconds();
	bool		bTimeLimitReached							= false;
	// Depending on platform FPlatformTime::Seconds might take a noticeable amount of time if called thousands of times so we avoid
	// enforcing the time limit too often, especially as neither Destroy nor actual deletion should take significant
	// amounts of time.
	const int32	TimeLimitEnforcementGranularityForDestroy	= 10;
	const int32	TimeLimitEnforcementGranularityForDeletion	= 100;

	if( !GObjFinishDestroyHasBeenRoutedToAllObjects && !bTimeLimitReached )
	{
		// Try to dispatch all FinishDestroy messages to unreachable objects.  We'll iterate over every
		// single object and destroy any that are ready to be destroyed.  The objects that aren't yet
		// ready will be added to a list to be processed afterwards.
		int32 TimePollCounter = 0;
		if (GObjCurrentPurgeObjectIndexNeedsReset)
		{
			// iterators don't have an op=, so we destroy it and reconstruct it with a placement new
			// GObjCurrentPurgeObjectIndex	= FRawObjectIterator(GObjCurrentPurgeObjectIndexResetPastPermanent);
			GObjCurrentPurgeObjectIndex.~FRawObjectIterator();
			new (&GObjCurrentPurgeObjectIndex) FRawObjectIterator(GObjCurrentPurgeObjectIndexResetPastPermanent);
			GObjCurrentPurgeObjectIndexNeedsReset = false;
		}

		while( GObjCurrentPurgeObjectIndex )
		{
			FUObjectItem* ObjectItem = *GObjCurrentPurgeObjectIndex;
			checkSlow(ObjectItem);

			//@todo UE4 - A prefetch was removed here. Re-add it. It wasn't right anyway, since it was ten items ahead and the consoles on have 8 prefetch slots

			if (ObjectItem->IsUnreachable())
			{
				UObject* Object = static_cast<UObject*>(ObjectItem->Object);
				// Object should always have had BeginDestroy called on it and never already be destroyed
				check( Object->HasAnyFlags( RF_BeginDestroyed ) && !Object->HasAnyFlags( RF_FinishDestroyed ) );

				// Only proceed with destroying the object if the asynchronous cleanup started by BeginDestroy has finished.
				if(Object->IsReadyForFinishDestroy())
				{
#if PERF_DETAILED_PER_CLASS_GC_STATS
					// Keep track of how many objects of a certain class we're purging.
					const FName& ClassName = Object->GetClass()->GetFName();
					int32 InstanceCount = GClassToPurgeCountMap.FindRef( ClassName );
					GClassToPurgeCountMap.Add( ClassName, ++InstanceCount );
#endif
					// Send FinishDestroy message.
					Object->ConditionalFinishDestroy();
				}
				else
				{
					// The object isn't ready for FinishDestroy to be called yet.  This is common in the
					// case of a graphics resource that is waiting for the render thread "release fence"
					// to complete.  Just calling IsReadyForFinishDestroy may begin the process of releasing
					// a resource, so we don't want to block iteration while waiting on the render thread.

					// Add the object index to our list of objects to revisit after we process everything else
					GGCObjectsPendingDestruction.Add(Object);
					GGCObjectsPendingDestructionCount++;
				}
			}

			// We've processed the object so increment our global iterator.  It's important to do this before
			// we test for the time limit so that we don't process the same object again next tick!
			++GObjCurrentPurgeObjectIndex;

			// Only check time limit every so often to avoid calling FPlatformTime::Seconds too often.
			const bool bPollTimeLimit = ((TimePollCounter++) % TimeLimitEnforcementGranularityForDestroy == 0);
			if( bUseTimeLimit && bPollTimeLimit && ((FPlatformTime::Seconds() - StartTime) > TimeLimit) )
			{
				bTimeLimitReached = true;
				break;
			}
		}

		// Have we finished the first round of attempting to call FinishDestroy on unreachable objects?
		if( !GObjCurrentPurgeObjectIndex )
		{
			// We've finished iterating over all unreachable objects, but we need still need to handle
			// objects that were deferred.
			while( GGCObjectsPendingDestructionCount > 0 )
			{
				int32 CurPendingObjIndex = 0;
				while( CurPendingObjIndex < GGCObjectsPendingDestructionCount )
				{
					// Grab the actual object for the current pending object list iteration
					UObject* Object = GGCObjectsPendingDestruction[ CurPendingObjIndex ];

					// Object should never have been added to the list if it failed this criteria
					check( Object != NULL && Object->IsUnreachable() );

					// Object should always have had BeginDestroy called on it and never already be destroyed
					check( Object->HasAnyFlags( RF_BeginDestroyed ) && !Object->HasAnyFlags( RF_FinishDestroyed ) );

					// Only proceed with destroying the object if the asynchronous cleanup started by BeginDestroy has finished.
					if( Object->IsReadyForFinishDestroy() )
					{
#if PERF_DETAILED_PER_CLASS_GC_STATS
						// Keep track of how many objects of a certain class we're purging.
						const FName& ClassName = Object->GetClass()->GetFName();
						int32 InstanceCount = GClassToPurgeCountMap.FindRef( ClassName );
						GClassToPurgeCountMap.Add( ClassName, ++InstanceCount );
#endif
						// Send FinishDestroy message.
						Object->ConditionalFinishDestroy();

						// Remove the object index from our list quickly (by swapping with the last object index).
						// NOTE: This is much faster than calling TArray.RemoveSwap and avoids shrinking allocations
						{
							// Swap the last index into the current index
							GGCObjectsPendingDestruction[ CurPendingObjIndex ] = GGCObjectsPendingDestruction[ GGCObjectsPendingDestructionCount - 1 ];

							// Decrement the object count
							GGCObjectsPendingDestructionCount--;
						}
					}
					else
					{
						// We'll revisit this object the next time around.  Move on to the next.
						CurPendingObjIndex++;
					}

					// Only check time limit every so often to avoid calling FPlatformTime::Seconds too often.
					const bool bPollTimeLimit = ((TimePollCounter++) % TimeLimitEnforcementGranularityForDestroy == 0);
					if( bUseTimeLimit && bPollTimeLimit && ((FPlatformTime::Seconds() - StartTime) > TimeLimit) )
					{
						bTimeLimitReached = true;
						break;
					}
				}

				if( bUseTimeLimit )
				{
					// A time limit is set and we've completed a full iteration over all leftover objects, so
					// go ahead and bail out even if we have more time left or objects left to process.  It's
					// likely in this case that we're waiting for the render thread.
					break;
				}
				else if( GGCObjectsPendingDestructionCount > 0 )
				{
					// Sleep before the next pass to give the render thread some time to release fences.
					FPlatformProcess::Sleep( 0 );
				}
			}

			// Have all objects been destroyed now?
			if( GGCObjectsPendingDestructionCount == 0 )
			{
				// Release memory we used for objects pending destruction, leaving some slack space
				GGCObjectsPendingDestruction.Empty( 256 );

				// Destroy has been routed to all objects so it's safe to delete objects now.
				GObjFinishDestroyHasBeenRoutedToAllObjects = true;
				GObjCurrentPurgeObjectIndexNeedsReset = true;
				GObjCurrentPurgeObjectIndexResetPastPermanent = !GExitPurge;
			}
		}
	}		

	if( GObjFinishDestroyHasBeenRoutedToAllObjects && !bTimeLimitReached )
	{
		// Perform actual object deletion.
		// @warning: Can't use FObjectIterator here because classes may be destroyed before objects.
		int32 ProcessCount = 0;
		if (GObjCurrentPurgeObjectIndexNeedsReset)
		{
			// iterators don't have an op=, so we destroy it and reconstruct it with a placement new
			// GObjCurrentPurgeObjectIndex	= FRawObjectIterator(GObjCurrentPurgeObjectIndexResetPastPermanent);
			GObjCurrentPurgeObjectIndex.~FRawObjectIterator();
			new (&GObjCurrentPurgeObjectIndex) FRawObjectIterator(GObjCurrentPurgeObjectIndexResetPastPermanent);
			GObjCurrentPurgeObjectIndexNeedsReset = false;
		}
		while( GObjCurrentPurgeObjectIndex )
		{
			//@todo UE4 - A prefetch was removed here. Re-add it. It wasn't right anyway, since it was ten items ahead and the consoles on have 8 prefetch slots

			FUObjectItem* ObjectItem = *GObjCurrentPurgeObjectIndex;
			checkSlow(ObjectItem);
			if (ObjectItem->IsUnreachable())
			{
				UObject* Object = (UObject*)ObjectItem->Object;
				check(Object->HasAllFlags(RF_FinishDestroyed|RF_BeginDestroyed));
				GIsPurgingObject				= true; 
				Object->~UObject();
				GUObjectAllocator.FreeUObject(Object);
				GIsPurgingObject				= false;
				// Keep track of purged stats.
				GPurgedObjectCountSinceLastMarkPhase++;
			}

			// Advance to the next object.
			++GObjCurrentPurgeObjectIndex;

			ProcessCount++;

			// Only check time limit every so often to avoid calling FPlatformTime::Seconds too often.
			if( bUseTimeLimit && (ProcessCount == TimeLimitEnforcementGranularityForDeletion))
			{
				if ((FPlatformTime::Seconds() - StartTime) > TimeLimit)
				{
					bTimeLimitReached = true;
					break;
				}
				ProcessCount = 0;
			}
		}

		if( !GObjCurrentPurgeObjectIndex )
		{
			// Incremental purge is finished, time to reset variables.
			GObjIncrementalPurgeIsInProgress				= false;
			GObjFinishDestroyHasBeenRoutedToAllObjects		= false;
			GObjPurgeIsRequired								= false;
			GObjCurrentPurgeObjectIndexNeedsReset			= true;
			GObjCurrentPurgeObjectIndexResetPastPermanent	= true;

			// Log status information.
			UE_LOG(LogGarbage, Log, TEXT("GC purged %i objects (%i -> %i)"), GPurgedObjectCountSinceLastMarkPhase, GObjectCountDuringLastMarkPhase, GObjectCountDuringLastMarkPhase - GPurgedObjectCountSinceLastMarkPhase );

#if PERF_DETAILED_PER_CLASS_GC_STATS
			LogClassCountInfo( TEXT("objects of"), GClassToPurgeCountMap, 10, GPurgedObjectCountSinceLastMarkPhase );
#endif
		}
	}
}

/**
 * Returns whether an incremental purge is still pending/ in progress.
 *
 * @return	true if incremental purge needs to be kicked off or is currently in progress, false othwerise.
 */
bool IsIncrementalPurgePending()
{
	return GObjIncrementalPurgeIsInProgress || GObjPurgeIsRequired;
}

/** Callback used by the editor to */
typedef void (*EditorPostReachabilityAnalysisCallbackType)();
COREUOBJECT_API EditorPostReachabilityAnalysisCallbackType EditorPostReachabilityAnalysisCallback = NULL;

// Allow parallel GC to be overridden to single threaded via console command.
#if !PLATFORM_MAC || !WITH_EDITORONLY_DATA
	static int32 GAllowParallelGC = 1;
#else
	static int32 GAllowParallelGC = 0;
#endif

static FAutoConsoleVariableRef CVarAllowParallelGC(
	TEXT("gc.AllowParallelGC"),
	GAllowParallelGC,
	TEXT("sed to control parallel GC."),
	ECVF_Default
	);

// This counts how many times GC was skipped
static int32 GNumAttemptsSinceLastGC = 0;

// Number of times GC can be skipped.
static int32 GNumRetriesBeforeForcingGC = 0;
static FAutoConsoleVariableRef CVarNumRetriesBeforeForcingGC(
	TEXT("gc.NumRetriesBeforeForcingGC"),
	GNumRetriesBeforeForcingGC,
	TEXT("Maximum number of times GC can be skipped if worker threads are currently modifying UObject state."),
	ECVF_Default
	);

// Force flush streaming on GC console variable
static int32 GFlushStreamingOnGC = 0;
static FAutoConsoleVariableRef CVarFlushStreamingOnGC(
	TEXT("gc.FlushStreamingOnGC"),
	GFlushStreamingOnGC,
	TEXT("If enabled, streaming will be flushed each time garbage collection is triggered."),
	ECVF_Default
	);

bool VerifyClusterAssumptions(UObject* ClusterRootObject);

/** 
 * Deletes all unreferenced objects, keeping objects that have any of the passed in KeepFlags set
 *
 * @param	KeepFlags			objects with those flags will be kept regardless of being referenced or not
 * @param	bPerformFullPurge	if true, perform a full purge after the mark pass
 */
void CollectGarbageInternal(EObjectFlags KeepFlags, bool bPerformFullPurge)
{
	SCOPE_TIME_GUARD(TEXT("Collect Garbage"));

	CheckImageIntegrityAtRuntime();

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "CollectGarbageInternal" ), STAT_CollectGarbageInternal, STATGROUP_GC );
	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, TEXT( "GarbageCollection - Begin" ) );

	// We can't collect garbage while there's a load in progress. E.g. one potential issue is Import.XObject
	check(!IsLoading());

	// Reset GC skip counter
	GNumAttemptsSinceLastGC = 0;

	// Flush streaming before GC if requested
	if (GFlushStreamingOnGC)
	{
		GGarbageCollectionGuardCritical.GCUnlock();
		FlushAsyncLoading();
		GGarbageCollectionGuardCritical.GCLock();
	}

	// Route callbacks so we can ensure that we are e.g. not in the middle of loading something by flushing
	// the async loading, etc...
	FCoreUObjectDelegates::PreGarbageCollect.Broadcast();
	GLastGCFrame = GFrameCounter;

	{
		// Set 'I'm garbage collecting' flag - might be checked inside various functions.
		// This has to be unlocked before we call post GC callbacks
		FGCScopeLock GCLock;

		UE_LOG(LogGarbage, Log, TEXT("Collecting garbage"));

		// Make sure previous incremental purge has finished or we do a full purge pass in case we haven't kicked one
		// off yet since the last call to garbage collection.
		if (GObjIncrementalPurgeIsInProgress || GObjPurgeIsRequired)
		{
			IncrementalPurgeGarbage(false);
			FMemory::Trim();
		}
		check(!GObjIncrementalPurgeIsInProgress);
		check(!GObjPurgeIsRequired);

#if VERIFY_DISREGARD_GC_ASSUMPTIONS
		FUObjectArray& UObjectArray = GUObjectArray;
		// Only verify assumptions if option is enabled. This avoids false positives in the Editor or commandlets.
		if ((UObjectArray.DisregardForGCEnabled() || GUObjectClusters.Num()) && GShouldVerifyGCAssumptions)
		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CollectGarbageInternal.VerifyGCAssumptions"), STAT_CollectGarbageInternal_VerifyGCAssumptions, STATGROUP_GC);
			bool bShouldAssert = false;

			// Verify that objects marked to be disregarded for GC are not referencing objects that are not part of the root set.
			for (FRawObjectIterator It(false); It; ++It)
			{
				FUObjectItem* ObjectItem = *It;
				UObject* Object = (UObject*)ObjectItem->Object;
				// Don't require UGCObjectReferencer's references to adhere to the assumptions.
				// Although we want the referencer itself to sit in the disregard for gc set, most of the objects
				// it's referencing will not be in the root set.
				if (UObjectArray.IsDisregardForGC(Object) && !Object->IsA(UGCObjectReferencer::StaticClass()))
				{
					// Serialize object with reference collector.
					TArray<UObject*> CollectedReferences;
					FReferenceFinder ObjectReferenceCollector(CollectedReferences);
					ObjectReferenceCollector.FindReferences(Object);

					// Iterate over referenced objects, finding bad ones.
					for (int32 ReferenceIndex = 0; ReferenceIndex < CollectedReferences.Num(); ReferenceIndex++)
					{
						UObject* ReferencedObject = CollectedReferences[ReferenceIndex];
						if (ReferencedObject &&
							!(ReferencedObject->IsRooted() ||
								UObjectArray.IsDisregardForGC(ReferencedObject) ||
								UObjectArray.ObjectToObjectItem(ReferencedObject)->GetOwnerIndex() ||
								UObjectArray.ObjectToObjectItem(ReferencedObject)->HasAnyFlags(EInternalObjectFlags::ClusterRoot)))
						{
							UE_LOG(LogGarbage, Warning, TEXT("Disregard for GC object %s referencing %s which is not part of root set"),
								*Object->GetFullName(),
								*ReferencedObject->GetFullName());
							bShouldAssert = true;
						}
					}
				}
				else if (ObjectItem->HasAnyFlags(EInternalObjectFlags::ClusterRoot))
				{
					if (!VerifyClusterAssumptions(Object))
					{
						bShouldAssert = true;
					}
				}
			}
			// Assert if we encountered any objects breaking implicit assumptions.
			if (bShouldAssert)
			{
				UE_LOG(LogGarbage, Fatal, TEXT("Encountered object(s) breaking Disregard for GC assumption. Please check log for details."));
			}
		}
#endif

		// Fall back to single threaded GC if processor count is 1 or parallel GC is disabled
		// or detailed per class gc stats are enabled (not thread safe)
		// Temporarily forcing single-threaded GC in the editor until Modify() can be safely removed from HandleObjectReference.
		const bool bForceSingleThreadedGC = !FApp::ShouldUseThreadingForPerformance() || !FPlatformProcess::SupportsMultithreading() ||
#if PLATFORM_SUPPORTS_MULTITHREADED_GC
		(FPlatformMisc::NumberOfCores() < 2 || GAllowParallelGC == 0 || PERF_DETAILED_PER_CLASS_GC_STATS);
#else	//PLATFORM_SUPPORTS_MULTITHREADED_GC
			true;
#endif	//PLATFORM_SUPPORTS_MULTITHREADED_GC

		// Perform reachability analysis.
		{
			const double StartTime = FPlatformTime::Seconds();
			FRealtimeGC TagUsedRealtimeGC;
			TagUsedRealtimeGC.PerformReachabilityAnalysis(KeepFlags, bForceSingleThreadedGC);
			UE_LOG(LogGarbage, Log, TEXT("%f ms for GC"), (FPlatformTime::Seconds() - StartTime) * 1000);
		}

#if WITH_EDITOR
		if (GIsEditor && EditorPostReachabilityAnalysisCallback)
		{
			EditorPostReachabilityAnalysisCallback();
		}
#endif // WITH_EDITOR

		{
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CollectGarbageInternal.UnhashUnreachable"), STAT_CollectGarbageInternal_UnhashUnreachable, STATGROUP_GC);

			TGuardValue<bool> GuardObjUnhashUnreachableIsInProgress(GObjUnhashUnreachableIsInProgress, true);

			// Unhash all unreachable objects.
			const double StartTime = FPlatformTime::Seconds();
			int32 ClustersRemoved = 0;
			for (FRawObjectIterator It(true); It; ++It)
			{
				//@todo UE4 - A prefetch was removed here. Re-add it. It wasn't right anyway, since it was ten items ahead and the consoles on have 8 prefetch slots

				FUObjectItem* ObjectItem = *It;
				checkSlow(ObjectItem);
				if (ObjectItem->IsUnreachable())
				{
					if ((ObjectItem->GetFlags() & EInternalObjectFlags::ClusterRoot) == EInternalObjectFlags::ClusterRoot)
					{
						// Nuke the entire cluster
						ObjectItem->ClearFlags(EInternalObjectFlags::ClusterRoot | EInternalObjectFlags::NoStrongReference);
						const int32 ClusterRootIndex = It.GetIndex();
						FUObjectCluster* Cluster = GUObjectClusters.FindChecked(ClusterRootIndex);
						checkSlow(Cluster);
						for (int32 ClusterObjectIndex : Cluster->Objects)
						{
							FUObjectItem* ClusterObjectItem = GUObjectArray.IndexToObjectUnsafeForGC(ClusterObjectIndex);
							ClusterObjectItem->ClearFlags(EInternalObjectFlags::NoStrongReference);
							ClusterObjectItem->SetOwnerIndex(0);

							if (!ClusterObjectItem->HasAnyFlags(EInternalObjectFlags::ReachableInCluster))
							{
								ClusterObjectItem->SetFlags(EInternalObjectFlags::Unreachable);
								if (ClusterObjectIndex < ClusterRootIndex)
								{
									UObject* ClusterObject = (UObject*)ClusterObjectItem->Object;
									ClusterObject->ConditionalBeginDestroy();
								}
							}
						}
						delete Cluster;
						GUObjectClusters.Remove(ClusterRootIndex);
						ClustersRemoved++;
					}

					// Begin the object's asynchronous destruction.
					UObject* Object = (UObject*)ObjectItem->Object;
					Object->ConditionalBeginDestroy();
				}
				else if (ObjectItem->IsNoStrongReference())
				{
					ObjectItem->ClearNoStrongReference();
					ObjectItem->SetPendingKill();
				}
			}
			UE_LOG(LogGarbage, Log, TEXT("%f ms for unhashing unreachable objects. Clusters removed: %d."), (FPlatformTime::Seconds() - StartTime) * 1000, ClustersRemoved);
		}

		// Set flag to indicate that we are relying on a purge to be performed.
		GObjPurgeIsRequired = true;
		// Reset purged count.
		GPurgedObjectCountSinceLastMarkPhase = 0;

		// Perform a full purge by not using a time limit for the incremental purge. The Editor always does a full purge.
		if (bPerformFullPurge || GIsEditor)
		{
			IncrementalPurgeGarbage(false);
		}
	}

	// Destroy all pending delete linkers
	DeleteLoaders();

	FMemory::Trim();

	// Route callbacks to verify GC assumptions
	FCoreUObjectDelegates::PostGarbageCollect.Broadcast();

	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, TEXT( "GarbageCollection - End" ) );
}

void CollectGarbage(EObjectFlags KeepFlags, bool bPerformFullPurge)
{
	// No other thread may be performing UOBject operations while we're running
	GGarbageCollectionGuardCritical.GCLock();

	// Perform actual garbage collection
	CollectGarbageInternal(KeepFlags, bPerformFullPurge);

	// Other threads are free to use UObjects
	GGarbageCollectionGuardCritical.GCUnlock();
}

bool TryCollectGarbage(EObjectFlags KeepFlags, bool bPerformFullPurge)
{
	// No other thread may be performing UOBject operations while we're running
	bool bCanRunGC = GGarbageCollectionGuardCritical.TryGCLock();
	if (!bCanRunGC)
	{
		if (GNumRetriesBeforeForcingGC > 0 && GNumAttemptsSinceLastGC > GNumRetriesBeforeForcingGC)
		{
			// Force GC and block main thread
			bCanRunGC = true;
			UE_LOG(LogGarbage, Warning, TEXT("TryCollectGarbage: forcing GC after %d skipped attempts."), GNumAttemptsSinceLastGC)
		}
	}
	if (bCanRunGC)
	{
		// Perform actual garbage collection
		CollectGarbageInternal(KeepFlags, bPerformFullPurge);

		// Other threads are free to use UObjects
		GGarbageCollectionGuardCritical.GCUnlock();
	}
	else
	{
		GNumAttemptsSinceLastGC++;
	}

	return bCanRunGC;
}


/**
 * Helper function to add referenced objects via serialization
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
#if TEST_ARO_FINDS_ALL_OBJECTS
static void AddReferencedObjectsViaSerialization( UObject* Object, FReferenceCollector& Collector )
{
	check( Object != NULL );

	// Collect object references by serializing the object
	FSimpleObjectReferenceCollectorArchive ObjectReferenceCollector( Object, Collector );
	Object->Serialize( ObjectReferenceCollector );
}
#endif

void UObject::CallAddReferencedObjects(FReferenceCollector& Collector)
{
	GetClass()->CallAddReferencedObjects(this, Collector);
}

void UObject::AddReferencedObjects(UObject* This, FReferenceCollector& Collector)
{
#if WITH_EDITOR
	//@todo UE4 - This seems to be required and it should not be. Seems to be related to the texture streamer.
	FLinkerLoad* LinkerLoad = This->GetLinker();	
	if (LinkerLoad)
	{
		LinkerLoad->AddReferencedObjects(Collector);
	}
	// Required by the unified GC when running in the editor
	if (GIsEditor)
	{
		UObject* LoadOuter = This->GetOuter();
		UClass *Class = This->GetClass();
		Collector.AllowEliminatingReferences(false);
		Collector.AddReferencedObject( LoadOuter, This );
		Collector.AllowEliminatingReferences(true);
		Collector.AddReferencedObject( Class, This );

#if TEST_ARO_FINDS_ALL_OBJECTS
		TArray<UObject*> SerializedObjects;
		AddReferencedObjectsViaSerialization( This, SerializedObjects );
		for( int32 Index = 0; Index < SerializedObjects.Num(); Index++ )
		{
			UObject* SerializedObject = SerializedObjects( Index );
			if( SerializedObject && SerializedObject->HasAnyFlags( RF_Unreachable ) )
			{
				UE_LOG( LogObj, Fatal, TEXT("Object %s references object %s which was not referenced via AddReferencedObjects."), *GetFullName(), *SerializedObject->GetFullName() );
			}
		}
#endif
	}
#endif
}

/*-----------------------------------------------------------------------------
	Implementation of realtime garbage collection helper functions in 
	UProperty, UClass, ...
-----------------------------------------------------------------------------*/

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return true if property (or sub- properties) contain a UObject reference, false otherwise
 */
bool UProperty::ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const
{
	return false;
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return true if property (or sub- properties) contain a UObject reference, false otherwise
 */
bool UArrayProperty::ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const
{
	check(Inner);
	return Inner->ContainsObjectReference(EncounteredStructProps);
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return true if property (or sub- properties) contain a UObject reference, false otherwise
 */
bool UMapProperty::ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const
{
	check(KeyProp);
	check(ValueProp);
	return KeyProp->ContainsObjectReference(EncounteredStructProps) || ValueProp->ContainsObjectReference(EncounteredStructProps);
}

/**
* Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
* UObject reference.
*
* @return true if property (or sub- properties) contain a UObject reference, false otherwise
*/
bool USetProperty::ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const
{
	check(ElementProp);
	return ElementProp->ContainsObjectReference(EncounteredStructProps);
}

/**
 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
 * UObject reference.
 *
 * @return true if property (or sub- properties) contain a UObject reference, false otherwise
 */
bool UStructProperty::ContainsObjectReference(TArray<const UStructProperty*>& EncounteredStructProps) const
{
	if (EncounteredStructProps.Contains(this))
	{
		return false;
	}
	else
	{
		if (!Struct)
		{
			UE_LOG(LogGarbage, Warning, TEXT("Broken UStructProperty does not have a UStruct: %s"), *GetFullName() );
		}
		else
		{
			EncounteredStructProps.Add(this);
			UProperty* Property = Struct->PropertyLink;
			while( Property )
			{
				if (Property->ContainsObjectReference(EncounteredStructProps))
				{
					EncounteredStructProps.RemoveSingleSwap(this);
					return true;
				}
				Property = Property->PropertyLinkNext;
			}
			EncounteredStructProps.RemoveSingleSwap(this);
		}
		return false;
	}
}

// Returns true if this property contains a weak UObject reference.
bool UProperty::ContainsWeakObjectReference() const
{
	return false;
}

// Returns true if this property contains a weak UObject reference.
bool UArrayProperty::ContainsWeakObjectReference() const
{
	check(Inner);
	return Inner->ContainsWeakObjectReference();
}

// Returns true if this property contains a weak UObject reference.
bool UMapProperty::ContainsWeakObjectReference() const
{
	check(KeyProp);
	check(ValueProp);
	return KeyProp->ContainsWeakObjectReference() || ValueProp->ContainsWeakObjectReference();
}

// Returns true if this property contains a weak UObject reference.
bool USetProperty::ContainsWeakObjectReference() const
{
	check(ElementProp);
	return ElementProp->ContainsWeakObjectReference();
}

// Returns true if this property contains a weak UObject reference.
bool UStructProperty::ContainsWeakObjectReference() const
{
	// prevent recursion in the case of structs containing dynamic arrays of themselves
	static TArray<const UStructProperty*> EncounteredStructProps;

	if (!EncounteredStructProps.Contains(this))
	{
		if (!Struct)
		{
			UE_LOG(LogGarbage, Warning, TEXT("Broken UStructProperty does not have a UStruct: %s"), *GetFullName() );
		}
		else
		{
			EncounteredStructProps.Add(this);

			for (UProperty* Property = Struct->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
			{
				if (Property->ContainsWeakObjectReference())
				{
					EncounteredStructProps.RemoveSingleSwap(this);
					return true;
				}
			}

			EncounteredStructProps.RemoveSingleSwap(this);
		}
	}
	
	return false;
}

// Returns true if this property contains a weak UObject reference.
bool UDelegateProperty::ContainsWeakObjectReference() const
{
	return true;
}

// Returns true if this property contains a weak UObject reference.
bool UMulticastDelegateProperty::ContainsWeakObjectReference() const
{
	return true;
}

/**
 * Scope helper structure to emit tokens for fixed arrays in the case of ArrayDim (passed in count) being > 1.
 */
struct FGCReferenceFixedArrayTokenHelper
{
	/**
	 * Constructor, emitting necessary tokens for fixed arrays if count > 1 and also keeping track of count so 
	 * destructor can do the same.
	 *
	 * @param InReferenceTokenStream	Token stream to emit tokens to
	 * @param InOffset					offset into object/ struct
	 * @param InCount					array count
	 * @param InStride					array type stride (e.g. sizeof(struct) or sizeof(UObject*))
	 * @param InProperty                the property this array represents
	 */
	FGCReferenceFixedArrayTokenHelper(UClass& OwnerClass, int32 InOffset, int32 InCount, int32 InStride, const UProperty& InProperty)
		: ReferenceTokenStream(&OwnerClass.ReferenceTokenStream)
	,	Count(InCount)
	{
		if( InCount > 1 )
		{
			OwnerClass.EmitObjectReference(InOffset, InProperty.GetFName(), GCRT_FixedArray);

			OwnerClass.ReferenceTokenStream.EmitStride(InStride);
			OwnerClass.ReferenceTokenStream.EmitCount(InCount);
		}
	}

	/** Destructor, emitting return if ArrayDim > 1 */
	~FGCReferenceFixedArrayTokenHelper()
	{
		if( Count > 1 )
		{
			ReferenceTokenStream->EmitReturn();
		}
	}

private:
	/** Reference token stream used to emit to */
	FGCReferenceTokenStream*	ReferenceTokenStream;
	/** Size of fixed array */
	int32							Count;
};


/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps)
{
}

/**
 * Emits tokens used by realtime garbage collection code to passed in OwnerClass' ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UObjectProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps)
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, sizeof(UObject*), *this);
	OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), GetFName(), GCRT_Object);
}

/**
 * Emits tokens used by realtime garbage collection code to passed in OwnerClass' ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UArrayProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps)
{
	if (Inner->ContainsObjectReference(EncounteredStructProps))
	{
		if( Inner->IsA(UStructProperty::StaticClass()) )
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), GetFName(), GCRT_ArrayStruct);

			OwnerClass.ReferenceTokenStream.EmitStride(Inner->ElementSize);
			const uint32 SkipIndexIndex = OwnerClass.ReferenceTokenStream.EmitSkipIndexPlaceholder();
			Inner->EmitReferenceInfo(OwnerClass, 0, EncounteredStructProps);
			const uint32 SkipIndex = OwnerClass.ReferenceTokenStream.EmitReturn();
			OwnerClass.ReferenceTokenStream.UpdateSkipIndexPlaceholder(SkipIndexIndex, SkipIndex);
		}
		else if( Inner->IsA(UObjectProperty::StaticClass()) )
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), GetFName(), GCRT_ArrayObject);
		}
		else if( Inner->IsA(UInterfaceProperty::StaticClass()) )
		{
			OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), GetFName(), GCRT_ArrayStruct);

			OwnerClass.ReferenceTokenStream.EmitStride(Inner->ElementSize);
			const uint32 SkipIndexIndex = OwnerClass.ReferenceTokenStream.EmitSkipIndexPlaceholder();

			OwnerClass.EmitObjectReference(0, GetFName(), GCRT_Object);

			const uint32 SkipIndex = OwnerClass.ReferenceTokenStream.EmitReturn();
			OwnerClass.ReferenceTokenStream.UpdateSkipIndexPlaceholder(SkipIndexIndex, SkipIndex);
		}
		else
		{
			UE_LOG(LogGarbage, Fatal, TEXT("Encountered unknown property containing object or name reference: %s in %s"), *Inner->GetFullName(), *GetFullName() );
		}
	}
}


/**
 * Emits tokens used by realtime garbage collection code to passed in OwnerClass' ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UMapProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps)
{
	if (ContainsObjectReference(EncounteredStructProps))
	{
		OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), GetFName(), GCRT_AddTMapReferencedObjects);
		OwnerClass.ReferenceTokenStream.EmitPointer((const void*)this);
	}
}

/**
* Emits tokens used by realtime garbage collection code to passed in OwnerClass' ReferenceTokenStream. The offset emitted is relative
* to the passed in BaseOffset which is used by e.g. arrays of structs.
*/
void USetProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps)
{
	if (ContainsObjectReference(EncounteredStructProps))
	{
		OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), GetFName(), GCRT_AddTSetReferencedObjects);
		OwnerClass.ReferenceTokenStream.EmitPointer((const void*)this);
	}
}


/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UStructProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps)
{
	if (Struct->StructFlags & STRUCT_AddStructReferencedObjects)
	{
		UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
		check(CppStructOps); // else should not have STRUCT_AddStructReferencedObjects
		FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, ElementSize, *this);

		OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), GetFName(), GCRT_AddStructReferencedObjects);

		void *FunctionPtr = (void*)CppStructOps->AddStructReferencedObjects();
		OwnerClass.ReferenceTokenStream.EmitPointer(FunctionPtr);
		return;
	}
	check(Struct);
	if (ContainsObjectReference(EncounteredStructProps))
	{
		FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, ElementSize, *this);

		UProperty* Property = Struct->PropertyLink;
		while( Property )
		{
			Property->EmitReferenceInfo(OwnerClass, BaseOffset + GetOffset_ForGC(), EncounteredStructProps);
			Property = Property->PropertyLinkNext;
		}
	}
}

/**
 * Emits tokens used by realtime garbage collection code to passed in ReferenceTokenStream. The offset emitted is relative
 * to the passed in BaseOffset which is used by e.g. arrays of structs.
 */
void UInterfaceProperty::EmitReferenceInfo(UClass& OwnerClass, int32 BaseOffset, TArray<const UStructProperty*>& EncounteredStructProps)
{
	FGCReferenceFixedArrayTokenHelper FixedArrayHelper(OwnerClass, BaseOffset + GetOffset_ForGC(), ArrayDim, sizeof(FScriptInterface), *this);

	OwnerClass.EmitObjectReference(BaseOffset + GetOffset_ForGC(), GetFName(), GCRT_Object);
}

void UClass::EmitObjectReference(int32 Offset, const FName& DebugName, EGCReferenceType Kind)
{
	FGCReferenceInfo ObjectReference(Kind, Offset);
	int32 TokenIndex = ReferenceTokenStream.EmitReferenceInfo(ObjectReference);

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
	DebugTokenMap.MapToken(DebugName, Offset, TokenIndex);
#endif
}

void UClass::EmitObjectArrayReference(int32 Offset, const FName& DebugName)
{
	check(HasAnyClassFlags(CLASS_Intrinsic));
	EmitObjectReference(Offset, DebugName, GCRT_ArrayObject);
}

uint32 UClass::EmitStructArrayBegin(int32 Offset, const FName& DebugName, int32 Stride)
{
	check(HasAnyClassFlags(CLASS_Intrinsic));
	EmitObjectReference(Offset, DebugName, GCRT_ArrayStruct);
	ReferenceTokenStream.EmitStride(Stride);
	const uint32 SkipIndexIndex = ReferenceTokenStream.EmitSkipIndexPlaceholder();
	return SkipIndexIndex;
}

/**
 * Realtime garbage collection helper function used to indicate the end of an array of structs. The
 * index following the current one will be written to the passed in SkipIndexIndex in order to be
 * able to skip tokens for empty dynamic arrays.
 *
 * @param SkipIndexIndex
 */
void UClass::EmitStructArrayEnd( uint32 SkipIndexIndex )
{
	check( HasAnyClassFlags( CLASS_Intrinsic ) );
	const uint32 SkipIndex = ReferenceTokenStream.EmitReturn();
	ReferenceTokenStream.UpdateSkipIndexPlaceholder( SkipIndexIndex, SkipIndex );
}

void UClass::EmitFixedArrayBegin(int32 Offset, const FName& DebugName, int32 Stride, int32 Count)
{
	check(HasAnyClassFlags(CLASS_Intrinsic));
	EmitObjectReference(Offset, DebugName, GCRT_FixedArray);
	ReferenceTokenStream.EmitStride(Stride);
	ReferenceTokenStream.EmitCount(Count);
}

/**
 * Realtime garbage collection helper function used to indicated the end of a fixed array.
 */
void UClass::EmitFixedArrayEnd()
{
	check( HasAnyClassFlags( CLASS_Intrinsic ) );
	ReferenceTokenStream.EmitReturn();
}

void UClass::AssembleReferenceTokenStream(bool bForce)
{
	if (!HasAnyClassFlags(CLASS_TokenStreamAssembled) || bForce)
	{
		if (bForce)
		{
			ReferenceTokenStream.Empty();
#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)
			DebugTokenMap.Empty();
#endif
			ClassFlags &= ~CLASS_TokenStreamAssembled;
		}
		TArray<const UStructProperty*> EncounteredStructProps;

		// Iterate over properties defined in this class
		for( TFieldIterator<UProperty> It(this,EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			UProperty* Property = *It;
			Property->EmitReferenceInfo(*this, 0, EncounteredStructProps);
		}

		if (GetSuperClass())
		{
			// Make sure super class has valid token stream.
			GetSuperClass()->AssembleReferenceTokenStream();
			if (!GetSuperClass()->ReferenceTokenStream.IsEmpty())
			{
				// Prepend super's stream. This automatically handles removing the EOS token.
				PrependStreamWithSuperClass(*GetSuperClass());
			}
		}
		else
		{
			UObjectBase::EmitBaseReferences(this);
		}

#if !WITH_EDITOR
		// In no-editor builds UObject::ARO is empty, thus only classes
		// which implement their own ARO function need to have the ARO token generated.
		if (ClassAddReferencedObjects != &UObject::AddReferencedObjects)
#endif
		{
			check(ClassAddReferencedObjects != NULL);
			ReferenceTokenStream.ReplaceOrAddAddReferencedObjectsCall(ClassAddReferencedObjects);
		}
		if (ReferenceTokenStream.IsEmpty())
		{
			return;
		}

		// Emit end of stream token.
		static const FName EOSDebugName("EOS");
		EmitObjectReference(0, EOSDebugName, GCRT_EndOfStream);

		// Shrink reference token stream to proper size.
		ReferenceTokenStream.Shrink();

		check(!HasAnyClassFlags(CLASS_TokenStreamAssembled)); // recursion here is probably bad
		ClassFlags |= CLASS_TokenStreamAssembled;
	}
}


/**
 * Prepends passed in stream to existing one.
 *
 * @param Other	stream to concatenate
 */
void FGCReferenceTokenStream::PrependStream( const FGCReferenceTokenStream& Other )
{
	// Remove embedded EOS token if needed.
	TArray<uint32> TempTokens = Other.Tokens;
	FGCReferenceInfo EndOfStream( GCRT_EndOfStream, 0 );
	if( TempTokens.Last() == EndOfStream )
	{
		TempTokens.RemoveAt( TempTokens.Num() - 1 );
	}
	// TArray doesn't have a general '+' operator.
	TempTokens += Tokens;
	Tokens = TempTokens;
}

void FGCReferenceTokenStream::ReplaceOrAddAddReferencedObjectsCall(void (*AddReferencedObjectsPtr)(UObject*, class FReferenceCollector&))
{
	// Try to find exiting ARO pointer and replace it (to avoid removing and readding tokens).
	for (int32 TokenStreamIndex = 0; TokenStreamIndex < Tokens.Num(); ++TokenStreamIndex)
	{
		uint32 TokenIndex = (uint32)TokenStreamIndex;
		const EGCReferenceType TokenType = (EGCReferenceType)AccessReferenceInfo(TokenIndex).Type;
		// Read token type and skip additional data if present.
		switch (TokenType)
		{
		case GCRT_ArrayStruct:
			{
				// Skip stride and move to Skip Info
				TokenIndex += 2;
				const FGCSkipInfo SkipInfo = ReadSkipInfo(TokenIndex);
				// Set the TokenIndex to the skip index - 1 because we're going to
				// increment in the for loop anyway.
				TokenIndex = SkipInfo.SkipIndex - 1;
			}
			break;
		case GCRT_FixedArray:
			{
				// Skip stride
				TokenIndex++; 
				// Skip count
				TokenIndex++; 
			}
			break;
		case GCRT_AddStructReferencedObjects:
			{
				// Skip pointer
				TokenIndex += GNumTokensPerPointer;
			}
			break;
		case GCRT_AddReferencedObjects:
			{
				// Store the pointer after the ARO token.
				StorePointer(&Tokens[++TokenIndex], (const void*)AddReferencedObjectsPtr);
				return;
			}
		case GCRT_AddTMapReferencedObjects:
		case GCRT_AddTSetReferencedObjects:
			{
				// Skip pointer
				TokenIndex += GNumTokensPerPointer;
			}
			break;
		case GCRT_None:
		case GCRT_Object:
		case GCRT_PersistentObject:
		case GCRT_ArrayObject:
		case GCRT_EndOfPointer:
		case GCRT_EndOfStream:		
			break;
		default:
			UE_LOG(LogGarbage, Fatal, TEXT("Unknown token type (%u) when trying to add ARO token."), (uint32)TokenType);
			break;
		};
		TokenStreamIndex = (int32)TokenIndex;
	}
	// ARO is not in the token stream yet.
	EmitReferenceInfo(FGCReferenceInfo(GCRT_AddReferencedObjects, 0));
	EmitPointer((const void*)AddReferencedObjectsPtr);
}

int32 FGCReferenceTokenStream::EmitReferenceInfo(FGCReferenceInfo ReferenceInfo)
{
	return Tokens.Add(ReferenceInfo);
}

/**
 * Emit placeholder for aray skip index, updated in UpdateSkipIndexPlaceholder
 *
 * @return the index of the skip index, used later in UpdateSkipIndexPlaceholder
 */
uint32 FGCReferenceTokenStream::EmitSkipIndexPlaceholder()
{
	return Tokens.Add( E_GCSkipIndexPlaceholder );
}

/**
 * Updates skip index place holder stored and passed in skip index index with passed
 * in skip index. The skip index is used to skip over tokens in the case of an empty 
 * dynamic array.
 * 
 * @param SkipIndexIndex index where skip index is stored at.
 * @param SkipIndex index to store at skip index index
 */
void FGCReferenceTokenStream::UpdateSkipIndexPlaceholder( uint32 SkipIndexIndex, uint32 SkipIndex )
{
	check( SkipIndex > 0 && SkipIndex <= (uint32)Tokens.Num() );
	const FGCReferenceInfo& ReferenceInfo = Tokens[SkipIndex-1];
	check( ReferenceInfo.Type != GCRT_None );
	check( Tokens[SkipIndexIndex] == E_GCSkipIndexPlaceholder );
	check( SkipIndexIndex < SkipIndex );
	check( ReferenceInfo.ReturnCount >= 1 );
	FGCSkipInfo SkipInfo;
	SkipInfo.SkipIndex			= SkipIndex - SkipIndexIndex;
	// We need to subtract 1 as ReturnCount includes return from this array.
	SkipInfo.InnerReturnCount	= ReferenceInfo.ReturnCount - 1; 
	Tokens[SkipIndexIndex]		= SkipInfo;
}

/**
 * Emit count
 *
 * @param Count count to emit
 */
void FGCReferenceTokenStream::EmitCount( uint32 Count )
{
	Tokens.Add( Count );
}

void FGCReferenceTokenStream::EmitPointer( void const* Ptr )
{
	const int32 StoreIndex = Tokens.Num();
	Tokens.AddUninitialized(GNumTokensPerPointer);
	StorePointer(&Tokens[StoreIndex], Ptr);
	// Now inser the end of pointer marker, this will mostly be used for storing ReturnCount value
	// if the pointer was stored at the end of struct array stream.
	EmitReferenceInfo(FGCReferenceInfo(GCRT_EndOfPointer, 0));
}

/**
 * Emit stride
 *
 * @param Stride stride to emit
 */
void FGCReferenceTokenStream::EmitStride( uint32 Stride )
{
	Tokens.Add( Stride );
}

/**
 * Increase return count on last token.
 *
 * @return index of next token
 */
uint32 FGCReferenceTokenStream::EmitReturn()
{
	FGCReferenceInfo ReferenceInfo = Tokens.Last();
	check(ReferenceInfo.Type != GCRT_None);
	ReferenceInfo.ReturnCount++;
	Tokens.Last() = ReferenceInfo;
	return Tokens.Num();
}

#if !(UE_BUILD_TEST || UE_BUILD_SHIPPING)

void FGCDebugReferenceTokenMap::MapToken(const FName& DebugName, int32 Offset, int32 TokenIndex)
{
	if(TokenMap.Num() <= TokenIndex)
	{
		TokenMap.AddZeroed(TokenIndex - TokenMap.Num() + 1);

		auto& TokenInfo = TokenMap[TokenIndex];

		TokenInfo.Offset = Offset;
		TokenInfo.Name = DebugName;
	}
	else
	{
		// Token already mapped.
		checkNoEntry();
	}
}

void FGCDebugReferenceTokenMap::PrependWithSuperClass(const UClass& SuperClass)
{
	if (SuperClass.ReferenceTokenStream.Size() == 0)
	{
		return;
	}

	// Check if token stream is already ended with end-of-stream token. If so then something's wrong.
	checkSlow(TokenMap.Num() == 0 || TokenMap.Last().Name != "EOS");

	int32 OldTokenNumber = TokenMap.Num();
	int32 NewTokenOffset = SuperClass.ReferenceTokenStream.Size() - 1;
	TokenMap.AddZeroed(NewTokenOffset);

	for(int32 OldTokenIndex = OldTokenNumber - 1; OldTokenIndex >= 0; --OldTokenIndex)
	{
		TokenMap[OldTokenIndex + NewTokenOffset] = TokenMap[OldTokenIndex];
	}

	for(int32 NewTokenIndex = 0; NewTokenIndex < NewTokenOffset; ++NewTokenIndex)
	{
		TokenMap[NewTokenIndex] = SuperClass.DebugTokenMap.GetTokenInfo(NewTokenIndex);
	}
}

const FTokenInfo& FGCDebugReferenceTokenMap::GetTokenInfo(int32 TokenIndex) const
{
	return TokenMap[TokenIndex];
}
#endif