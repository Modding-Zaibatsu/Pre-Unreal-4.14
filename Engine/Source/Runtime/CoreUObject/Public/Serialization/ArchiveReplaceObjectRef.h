// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AsyncFileHandle.h"
#include "TextPackageNamespaceUtil.h"

/** Base class for object replacement archives */ 
class COREUOBJECT_API FArchiveReplaceObjectRefBase : public FArchiveUObject
{
protected:
	/**
	* Serializes a single object
	*/
	void SerializeObject(UObject* ObjectToSerialize);
};

/*----------------------------------------------------------------------------
	FArchiveReplaceObjectRef.
----------------------------------------------------------------------------*/
/**
 * Archive for replacing a reference to an object. This classes uses
 * serialization to replace all references to one object with another.
 * Note that this archive will only traverse objects with an Outer
 * that matches InSearchObject.
 *
 * NOTE: The template type must be a child of UObject or this class will not compile.
 */
template< class T >
class FArchiveReplaceObjectRef : public FArchiveReplaceObjectRefBase
{
public:
	/**
	 * Initializes variables and starts the serialization search
	 *
	 * @param InSearchObject		The object to start the search on
	 * @param ReplacementMap		Map of objects to find -> objects to replace them with (null zeros them)
	 * @param bNullPrivateRefs		Whether references to non-public objects not contained within the SearchObject
	 *								should be set to null
	 * @param bIgnoreOuterRef		Whether we should replace Outer pointers on Objects.
	 * @param bIgnoreArchetypeRef	Whether we should replace the ObjectArchetype reference on Objects.
	 * @param bDelayStart			Specify true to prevent the constructor from starting the process.  Allows child classes' to do initialization stuff in their ctor
	 */
	FArchiveReplaceObjectRef
	(
		UObject* InSearchObject,
		const TMap<T*,T*>& inReplacementMap,
		bool bNullPrivateRefs,
		bool bIgnoreOuterRef,
		bool bIgnoreArchetypeRef,
		bool bDelayStart = false,
		bool bIgnoreClassGeneratedByRef = true
	)
	: SearchObject(InSearchObject), ReplacementMap(inReplacementMap)
	, Count(0), bNullPrivateReferences(bNullPrivateRefs)
	{
		ArIsObjectReferenceCollector = true;
		ArIsModifyingWeakAndStrongReferences = true;		// Also replace weak references too!
		ArIgnoreArchetypeRef = bIgnoreArchetypeRef;
		ArIgnoreOuterRef = bIgnoreOuterRef;
		ArIgnoreClassGeneratedByRef = bIgnoreClassGeneratedByRef;

		if ( !bDelayStart )
		{
			SerializeSearchObject();
		}
	}

	/**
	 * Starts the serialization of the root object
	 */
	void SerializeSearchObject()
	{
		ReplacedReferences.Empty();

		if (SearchObject != NULL && !SerializedObjects.Find(SearchObject)
		&&	(ReplacementMap.Num() > 0 || bNullPrivateReferences))
		{
			// start the initial serialization
			SerializedObjects.Add(SearchObject);
			SerializeObject(SearchObject);
			for (int32 Iter = 0; Iter < PendingSerializationObjects.Num(); Iter++)
			{
				SerializeObject(PendingSerializationObjects[Iter]);
			}
			PendingSerializationObjects.Reset();
		}
	}

	/**
	 * Returns the number of times the object was referenced
	 */
	int64 GetCount() const
	{
		return Count;
	}

	/**
	 * Returns a reference to the object this archive is operating on
	 */
	const UObject* GetSearchObject() const { return SearchObject; }

	/**
	 * Returns a reference to the replaced references map
	 */
	const TMap<UObject*, TArray<UProperty*>>& GetReplacedReferences() const { return ReplacedReferences; }

	/**
	 * Serializes the reference to the object
	 */
	FArchive& operator<<( UObject*& Obj )
	{
		if (Obj != NULL)
		{
			// If these match, replace the reference
			T* const* ReplaceWith = (T*const*)((const TMap<UObject*,UObject*>*)&ReplacementMap)->Find(Obj);
			if ( ReplaceWith != NULL )
			{
				Obj = *ReplaceWith;
				ReplacedReferences.FindOrAdd(Obj).AddUnique(GetSerializedProperty());
				Count++;
			}
			// A->IsIn(A) returns false, but we don't want to NULL that reference out, so extra check here.
			else if ( Obj == SearchObject || Obj->IsIn(SearchObject) )
			{
#if 0
				// DEBUG: Log when we are using the A->IsIn(A) path here.
				if(Obj == SearchObject)
				{
					FString ObjName = Obj->GetPathName();
					UE_LOG(LogSerialization, Log,  TEXT("FArchiveReplaceObjectRef: Obj == SearchObject : '%s'"), *ObjName );
				}
#endif
				bool bAlreadyAdded = false;
				SerializedObjects.Add(Obj, &bAlreadyAdded);
				if (!bAlreadyAdded)
				{
					// No recursion
					PendingSerializationObjects.Add(Obj);
				}
			}
			else if ( bNullPrivateReferences && !Obj->HasAnyFlags(RF_Public) )
			{
				Obj = NULL;
			}
		}
		return *this;
	}

	/**
	 * Returns the name of this archive.
	 **/
	virtual FString GetArchiveName() const { return TEXT("ReplaceObjectRef"); }

protected:
	/** Initial object to start the reference search from */
	UObject* SearchObject;

	/** Map of objects to find references to -> object to replace references with */
	const TMap<T*,T*>& ReplacementMap;
	
	/** The number of times encountered */
	int32 Count;

	/** List of objects that have already been serialized */
	TSet<UObject*> SerializedObjects;

	/** Object that will be serialized */
	TArray<UObject*> PendingSerializationObjects;

	/** Map of referencing objects to referencing properties */
	TMap<UObject*, TArray<UProperty*>> ReplacedReferences;

	/**
	 * Whether references to non-public objects not contained within the SearchObject
	 * should be set to null
	 */
	bool bNullPrivateReferences;
};
