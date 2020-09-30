// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "PropertyHelper.h"
#include "ScopeExit.h"

namespace UE4MapProperty_Private
{
	/**
	 * Checks if any of the pairs in the map compare equal to the one passed.
	 *
	 * @param  MapHelper  The map to search through.
	 * @param  Index      The index in the map to start searching from.
	 * @param  Num        The number of elements to compare.
	 */
	bool AnyEqual(const FScriptMapHelper& MapHelper, int32 Index, int32 Num, const uint8* PairToCompare, uint32 PortFlags)
	{
		UProperty* KeyProp   = MapHelper.GetKeyProperty();
		UProperty* ValueProp = MapHelper.GetValueProperty();

		int32 ValueOffset = MapHelper.MapLayout.ValueOffset;

		for (; Num; --Num)
		{
			while (!MapHelper.IsValidIndex(Index))
			{
				++Index;
			}

			if (KeyProp->Identical(MapHelper.GetPairPtr(Index), PairToCompare, PortFlags) && ValueProp->Identical(MapHelper.GetPairPtr(Index) + ValueOffset, PairToCompare + ValueOffset, PortFlags))
			{
				return true;
			}

			++Index;
		}

		return false;
	}

	bool RangesContainSameAmountsOfVal(const FScriptMapHelper& MapHelperA, int32 IndexA, const FScriptMapHelper& MapHelperB, int32 IndexB, int32 Num, const uint8* PairToCompare, uint32 PortFlags)
	{
		UProperty* KeyProp   = MapHelperA.GetKeyProperty();
		UProperty* ValueProp = MapHelperA.GetValueProperty();

		// Ensure that both maps are the same type
		check(KeyProp   == MapHelperB.GetKeyProperty());
		check(ValueProp == MapHelperB.GetValueProperty());

		int32 ValueOffset = MapHelperA.MapLayout.ValueOffset;

		int32 CountA = 0;
		int32 CountB = 0;
		for (;;)
		{
			if (Num == 0)
			{
				return CountA == CountB;
			}

			while (!MapHelperA.IsValidIndex(IndexA))
			{
				++IndexA;
			}

			while (!MapHelperB.IsValidIndex(IndexB))
			{
				++IndexB;
			}

			const uint8* PairA = MapHelperA.GetPairPtr(IndexA);
			const uint8* PairB = MapHelperB.GetPairPtr(IndexB);
			if (KeyProp->Identical(PairA, PairToCompare, PortFlags) && ValueProp->Identical(PairA + ValueOffset, PairToCompare + ValueOffset, PortFlags))
			{
				++CountA;
			}

			if (KeyProp->Identical(PairB, PairToCompare, PortFlags) && ValueProp->Identical(PairB + ValueOffset, PairToCompare + ValueOffset, PortFlags))
			{
				++CountB;
			}

			++IndexA;
			++IndexB;
			--Num;
		}
	}

	bool IsPermutation(const FScriptMapHelper& MapHelperA, const FScriptMapHelper& MapHelperB, uint32 PortFlags)
	{
		UProperty* KeyProp   = MapHelperA.GetKeyProperty();
		UProperty* ValueProp = MapHelperA.GetValueProperty();

		// Ensure that both maps are the same type
		check(KeyProp   == MapHelperB.GetKeyProperty());
		check(ValueProp == MapHelperB.GetValueProperty());

		int32 Num = MapHelperA.Num();
		if (Num != MapHelperB.Num())
		{
			return false;
		}

		int32 ValueOffset = MapHelperA.MapLayout.ValueOffset;

		// Skip over common initial sequence
		int32 IndexA = 0;
		int32 IndexB = 0;
		for (;;)
		{
			if (Num == 0)
			{
				return true;
			}

			while (!MapHelperA.IsValidIndex(IndexA))
			{
				++IndexA;
			}

			while (!MapHelperB.IsValidIndex(IndexB))
			{
				++IndexB;
			}

			const uint8* PairA = MapHelperA.GetPairPtr(IndexA);
			const uint8* PairB = MapHelperB.GetPairPtr(IndexB);
			if (!KeyProp->Identical(PairA, PairB, PortFlags))
			{
				break;
			}

			if (!ValueProp->Identical(PairA + ValueOffset, PairB + ValueOffset, PortFlags))
			{
				break;
			}

			++IndexA;
			++IndexB;
			--Num;
		}

		int32 FirstIndexA = IndexA;
		int32 FirstIndexB = IndexB;
		int32 FirstNum    = Num;
		for (;;)
		{
			const uint8* PairA = MapHelperA.GetPairPtr(IndexA);
			if (!AnyEqual(MapHelperA, FirstIndexA, FirstNum - Num, PairA, PortFlags) && !RangesContainSameAmountsOfVal(MapHelperA, IndexA, MapHelperB, IndexB, Num, PairA, PortFlags))
			{
				return false;
			}

			--Num;
			if (Num == 0)
			{
				return true;
			}

			while (!MapHelperA.IsValidIndex(IndexA))
			{
				++IndexA;
			}

			while (!MapHelperB.IsValidIndex(IndexB))
			{
				++IndexB;
			}
		}
	}
}

UMapProperty::UMapProperty(const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags)
: UMapProperty_Super(ObjectInitializer, EC_CppProperty, InOffset, InFlags)
{
	// These are expected to be set post-construction by AddCppProperty
	KeyProp   = nullptr;
	ValueProp = nullptr;
}

void UMapProperty::LinkInternal(FArchive& Ar)
{
	check(KeyProp && ValueProp);

	if (auto* MyLinker = GetLinker())
	{
		MyLinker->Preload(this);
	}
	Ar.Preload(KeyProp);
	Ar.Preload(ValueProp);
	KeyProp  ->Link(Ar);
	ValueProp->Link(Ar);

	int32 KeySize        = KeyProp  ->GetSize();
	int32 ValueSize      = ValueProp->GetSize();
	int32 KeyAlignment   = KeyProp  ->GetMinAlignment();
	int32 ValueAlignment = ValueProp->GetMinAlignment();

	MapLayout = FScriptMap::GetScriptLayout(KeySize, KeyAlignment, ValueSize, ValueAlignment);

	ValueProp->SetOffset_Internal(MapLayout.ValueOffset);

	Super::LinkInternal(Ar);
}

bool UMapProperty::Identical(const void* A, const void* B, uint32 PortFlags) const
{
	checkSlow(KeyProp);
	checkSlow(ValueProp);

	FScriptMapHelper MapHelperA(this, A);

	int32 ANum = MapHelperA.Num();

	if (!B)
	{
		return ANum == 0;
	}

	FScriptMapHelper MapHelperB(this, B);
	if (ANum != MapHelperB.Num())
	{
		return false;
	}

	return UE4MapProperty_Private::IsPermutation(MapHelperA, MapHelperB, PortFlags);
}

void UMapProperty::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	OutDeps.Add(KeyProp);
	OutDeps.Add(ValueProp);
}

void UMapProperty::SerializeItem(FArchive& Ar, void* Value, const void* Defaults) const
{
	checkSlow(KeyProp);
	checkSlow(ValueProp);

	// Ensure that the key/value properties have been loaded before calling SerializeItem() on them
	Ar.Preload(KeyProp);
	Ar.Preload(ValueProp);

	FScriptMapHelper MapHelper(this, Value);

	if (Ar.IsLoading())
	{
		if (Defaults)
		{
			CopyValuesInternal(Value, Defaults, 1);
		}
		else
		{
			MapHelper.EmptyValues();
		}

		uint8* TempKeyStorage = nullptr;
		ON_SCOPE_EXIT
		{
			if (TempKeyStorage)
			{
				KeyProp->DestroyValue(TempKeyStorage);
				FMemory::Free(TempKeyStorage);
			}
		};

		// Delete any explicitly-removed keys
		int32 NumKeysToRemove = 0;
		Ar << NumKeysToRemove;
		if (NumKeysToRemove)
		{
			TempKeyStorage = (uint8*)FMemory::Malloc(MapLayout.SetLayout.Size);
			KeyProp->InitializeValue(TempKeyStorage);

			for (; NumKeysToRemove; --NumKeysToRemove)
			{
				// Read key into temporary storage
				KeyProp->SerializeItem(Ar, TempKeyStorage);

				// If the key is in the map, remove it
				int32 Found = MapHelper.FindMapIndexWithKey(TempKeyStorage);
				if (Found != INDEX_NONE)
				{
					MapHelper.RemoveAt_NeedsRehash(Found);
				}
			}
		}

		int32 Num = 0;
		Ar << Num;

		// Allocate temporary key space if we haven't allocated it already above
		if (Num != 0 && !TempKeyStorage)
		{
			TempKeyStorage = (uint8*)FMemory::Malloc(MapLayout.SetLayout.Size);
			KeyProp->InitializeValue(TempKeyStorage);
		}

		// Read remaining items into container
		for (; Num; --Num)
		{
			// Read key into temporary storage
			KeyProp->SerializeItem(Ar, TempKeyStorage);

			// Add a new default value if the key doesn't currently exist in the map
			int32 NextPairIndex = MapHelper.FindMapIndexWithKey(TempKeyStorage);
			if (NextPairIndex == INDEX_NONE)
			{
				NextPairIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
			}

			uint8* NextPairPtr = MapHelper.GetPairPtrWithoutCheck(NextPairIndex);

			// Copy over deserialised key from temporary storage
			KeyProp->CopyCompleteValue_InContainer(NextPairPtr, TempKeyStorage);

			// Deserialize value
			ValueProp->SerializeItem(Ar, NextPairPtr + MapLayout.ValueOffset);
		}

		MapHelper.Rehash();
	}
	else
	{
		FScriptMapHelper DefaultsHelper(this, Defaults);

		// Container for temporarily tracking some indices
		TSet<int32> Indices;

		// Determine how many keys are missing from the object
		if (Defaults)
		{
			for (int32 Index = 0, Count = DefaultsHelper.Num(); Count; ++Index)
			{
				uint8* DefaultPairPtr = DefaultsHelper.GetPairPtrWithoutCheck(Index);

				if (DefaultsHelper.IsValidIndex(Index))
				{
					if (!MapHelper.FindMapPairPtrWithKey(DefaultPairPtr))
					{
						Indices.Add(Index);
					}

					--Count;
				}
			}
		}

		// Write out the missing keys
		int32 MissingKeysNum = Indices.Num();
		Ar << MissingKeysNum;
		for (int32 Index : Indices)
		{
			KeyProp->SerializeItem(Ar, DefaultsHelper.GetPairPtr(Index));
		}

		// Write out differences from defaults
		if (Defaults)
		{
			Indices.Empty(Indices.Num());
			for (int32 Index = 0, Count = MapHelper.Num(); Count; ++Index)
			{
				if (MapHelper.IsValidIndex(Index))
				{
					uint8* ValuePairPtr   = MapHelper.GetPairPtrWithoutCheck(Index);
					uint8* DefaultPairPtr = DefaultsHelper.FindMapPairPtrWithKey(ValuePairPtr);

					if (!DefaultPairPtr || !ValueProp->Identical(ValuePairPtr + MapLayout.ValueOffset, DefaultPairPtr + MapLayout.ValueOffset))
					{
						Indices.Add(Index);
					}

					--Count;
				}
			}

			// Write out differences from defaults
			int32 Num = Indices.Num();
			Ar << Num;
			for (int32 Index : Indices)
			{
				uint8* ValuePairPtr = MapHelper.GetPairPtrWithoutCheck(Index);

				KeyProp  ->SerializeItem(Ar, ValuePairPtr);
				ValueProp->SerializeItem(Ar, ValuePairPtr + MapLayout.ValueOffset);
			}
		}
		else
		{
			int32 Num = MapHelper.Num();
			Ar << Num;
			for (int32 Index = 0; Num; ++Index)
			{
				if (MapHelper.IsValidIndex(Index))
				{
					uint8* ValuePairPtr = MapHelper.GetPairPtrWithoutCheck(Index);

					KeyProp  ->SerializeItem(Ar, ValuePairPtr);
					ValueProp->SerializeItem(Ar, ValuePairPtr + MapLayout.ValueOffset);

					--Num;
				}
			}
		}
	}
}

bool UMapProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	UE_LOG( LogProperty, Fatal, TEXT( "Deprecated code path" ) );
	return 1;
}

void UMapProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	Ar << KeyProp;
	Ar << ValueProp;
}

void UMapProperty::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UMapProperty* This = CastChecked<UMapProperty>(InThis);

	Collector.AddReferencedObject(This->KeyProp,   This);
	Collector.AddReferencedObject(This->ValueProp, This);

	Super::AddReferencedObjects(This, Collector);
}

FString UMapProperty::GetCPPType(FString* ExtendedTypeText, uint32 CPPExportFlags) const
{
	checkSlow(KeyProp);
	checkSlow(ValueProp);

	if (ExtendedTypeText)
	{
		FString KeyExtendedTypeText;
		FString KeyTypeText = KeyProp->GetCPPType(&KeyExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider map keys to be "arguments or return values"

		FString ValueExtendedTypeText;
		FString ValueTypeText = ValueProp->GetCPPType(&ValueExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider map values to be "arguments or return values"

		*ExtendedTypeText = FString::Printf(TEXT("<%s%s,%s%s>"), *KeyTypeText, *KeyExtendedTypeText, *ValueTypeText, *ValueExtendedTypeText);
	}

	return TEXT("TMap");
}

FString UMapProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	checkSlow(KeyProp);
	checkSlow(ValueProp);
	ExtendedTypeText = FString::Printf(TEXT("%s,%s"), *KeyProp->GetCPPType(), *ValueProp->GetCPPType());
	return TEXT("TMAP");
}

void UMapProperty::ExportTextItem(FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += TEXT("{}");
		return;
	}

	checkSlow(KeyProp);
	checkSlow(ValueProp);

	FScriptMapHelper MapHelper(this, PropertyValue);

	if (MapHelper.Num() == 0)
	{
		ValueStr += TEXT("()");
		return;
	}

	uint8* StructDefaults = nullptr;
	if (auto* StructValueProp = dynamic_cast<UStructProperty*>(ValueProp))
	{
		checkSlow(StructValueProp->Struct);

		StructDefaults = (uint8*)FMemory::Malloc(MapLayout.SetLayout.Size);
		ValueProp->InitializeValue(StructDefaults + MapLayout.ValueOffset);
	}
	ON_SCOPE_EXIT
	{
		if (StructDefaults)
		{
			ValueProp->DestroyValue(StructDefaults + MapLayout.ValueOffset);
			FMemory::Free(StructDefaults);
		}
	};

	FScriptMapHelper DefaultMapHelper(this, DefaultValue);

	uint8* PropData = MapHelper.GetPairPtrWithoutCheck(0);
	if (PortFlags & PPF_BlueprintDebugView)
	{
		int32 Index  = 0;
		bool  bFirst = true;
		for (int32 Count = MapHelper.Num(); Count; PropData += MapLayout.SetLayout.Size, ++Index)
		{
			if (MapHelper.IsValidIndex(Index))
			{
				if (bFirst)
				{
					bFirst = false;
				}
				else
				{
					ValueStr += TCHAR('\n');
				}

				ValueStr += TEXT("[");
				KeyProp->ExportTextItem(ValueStr, PropData, nullptr, Parent, PortFlags | PPF_Delimited, ExportRootScope);
				ValueStr += TEXT("] ");

				// Always use struct defaults if the inner is a struct, for symmetry with the import of array inner struct defaults
				uint8* PropDefault = StructDefaults ? StructDefaults : DefaultValue ? DefaultMapHelper.FindMapPairPtrWithKey(PropData) : nullptr;

				ValueProp->ExportTextItem(ValueStr, PropData + MapLayout.ValueOffset, PropDefault + MapLayout.ValueOffset, Parent, PortFlags | PPF_Delimited, ExportRootScope);

				--Count;
			}
		}
	}
	else
	{
		int32 Index  = 0;
		bool  bFirst = true;
		for (int32 Count = MapHelper.Num(); Count; PropData += MapLayout.SetLayout.Size, ++Index)
		{
			if (MapHelper.IsValidIndex(Index))
			{
				if (bFirst)
				{
					ValueStr += TCHAR('(');
					bFirst = false;
				}
				else
				{
					ValueStr += TCHAR(',');
				}

				ValueStr += TEXT("(");

				KeyProp->ExportTextItem(ValueStr, PropData, nullptr, Parent, PortFlags | PPF_Delimited, ExportRootScope);

				ValueStr += TEXT(", ");

				// Always use struct defaults if the inner is a struct, for symmetry with the import of array inner struct defaults
				uint8* PropDefault = StructDefaults ? StructDefaults : DefaultValue ? DefaultMapHelper.FindMapPairPtrWithKey(PropData) : nullptr;

				ValueProp->ExportTextItem(ValueStr, PropData + MapLayout.ValueOffset, PropDefault + MapLayout.ValueOffset, Parent, PortFlags | PPF_Delimited, ExportRootScope);

				ValueStr += TEXT(")");

				--Count;
			}
		}

		ValueStr += TEXT(")");
	}
}

const TCHAR* UMapProperty::ImportText_Internal(const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText) const
{
	checkSlow(KeyProp);
	checkSlow(ValueProp);

	FScriptMapHelper MapHelper(this, Data);
	MapHelper.EmptyValues();

	// If we export an empty array we export an empty string, so ensure that if we're passed an empty string
	// we interpret it as an empty array.
	if (*Buffer++ != TCHAR('('))
	{
		return nullptr;
	}

	SkipWhitespace(Buffer);
	if (*Buffer == TCHAR(')'))
	{
		return Buffer + 1;
	}

	int32 Index = 0;
	for (;;)
	{
		MapHelper.AddUninitializedValue();
		MapHelper.ConstructItem(Index);
		uint8* PairPtr = MapHelper.GetPairPtrWithoutCheck(Index);

		if (*Buffer++ != TCHAR('('))
		{
			return nullptr;
		}

		// Parse the key
		Buffer = KeyProp->ImportText(Buffer, PairPtr, PortFlags | PPF_Delimited, Parent, ErrorText);
		if (!Buffer)
		{
			return nullptr;
		}

		SkipWhitespace(Buffer);
		if (*Buffer++ != TCHAR(','))
		{
			return nullptr;
		}

		// Parse the value
		SkipWhitespace(Buffer);
		Buffer = ValueProp->ImportText(Buffer, PairPtr + MapLayout.ValueOffset, PortFlags | PPF_Delimited, Parent, ErrorText);
		if (!Buffer)
		{
			return nullptr;
		}

		SkipWhitespace(Buffer);
		if (*Buffer++ != TCHAR(')'))
		{
			return nullptr;
		}

		switch (*Buffer++)
		{
			case TCHAR(')'):
				MapHelper.Rehash();
				return Buffer;

			case TCHAR(','):
				break;

			default:
				return nullptr;
		}

		++Index;
	}
}

void UMapProperty::AddCppProperty( UProperty* Property )
{
	check(Property);

	if (Property->GetOffset_ForInternal() == 0)
	{
		// If it's at offset zero, assume it's the key
		check(!KeyProp);
		KeyProp = Property;
	}
	else
	{
		// Otherwise assume it's the value
		check(!ValueProp);
		ValueProp = Property;
	}
}

void UMapProperty::CopyValuesInternal(void* Dest, void const* Src, int32 Count) const
{
	check(Count == 1);

	FScriptMapHelper SrcMapHelper (this, Src);
	FScriptMapHelper DestMapHelper(this, Dest);

	int32 Num = SrcMapHelper.Num();
	DestMapHelper.EmptyValues(Num);

	if (Num == 0)
	{
		return;
	}

	for (int32 SrcIndex = 0; Num; ++SrcIndex)
	{
		if (SrcMapHelper.IsValidIndex(SrcIndex))
		{
			int32 DestIndex = DestMapHelper.AddDefaultValue_Invalid_NeedsRehash();

			uint8* SrcData  = SrcMapHelper .GetPairPtrWithoutCheck(SrcIndex);
			uint8* DestData = DestMapHelper.GetPairPtrWithoutCheck(DestIndex);

			KeyProp  ->CopyCompleteValue_InContainer(DestData, SrcData);
			ValueProp->CopyCompleteValue_InContainer(DestData, SrcData);

			--Num;
		}
	}

	DestMapHelper.Rehash();
}

void UMapProperty::ClearValueInternal(void* Data) const
{
	FScriptMapHelper MapHelper(this, Data);
	MapHelper.EmptyValues();
}

void UMapProperty::DestroyValueInternal(void* Data) const
{
	FScriptMapHelper MapHelper(this, Data);
	MapHelper.EmptyValues();

	//@todo UE4 potential double destroy later from this...would be ok for a script map, but still
	((FScriptMap*)Data)->~FScriptMap();
}

bool UMapProperty::PassCPPArgsByRef() const
{
	return true;
}

/**
 * Creates new copies of components
 * 
 * @param	Data				pointer to the address of the instanced object referenced by this UComponentProperty
 * @param	DefaultData			pointer to the address of the default value of the instanced object referenced by this UComponentProperty
 * @param	Owner				the object that contains this property's data
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates
 */
void UMapProperty::InstanceSubobjects(void* Data, void const* DefaultData, UObject* Owner, FObjectInstancingGraph* InstanceGraph)
{
	if (!Data)
	{
		return;
	}

	bool bInstancedKey   = KeyProp  ->ContainsInstancedObjectProperty();
	bool bInstancedValue = ValueProp->ContainsInstancedObjectProperty();

	if (!bInstancedKey && !bInstancedValue)
	{
		return;
	}

	FScriptMapHelper MapHelper(this, Data);

	if (DefaultData)
	{
		FScriptMapHelper DefaultMapHelper(this, DefaultData);
		int32            DefaultNum = DefaultMapHelper.Num();

		for (int32 Index = 0, Num = MapHelper.Num(); Num; ++Index)
		{
			if (MapHelper.IsValidIndex(Index))
			{
				uint8* PairPtr        = MapHelper.GetPairPtr(Index);
				uint8* DefaultPairPtr = DefaultMapHelper.FindMapPairPtrWithKey(PairPtr, Index);

				if (bInstancedKey)
				{
					KeyProp->InstanceSubobjects(PairPtr, DefaultPairPtr, Owner, InstanceGraph);
				}

				if (bInstancedValue)
				{
					ValueProp->InstanceSubobjects(PairPtr + MapLayout.ValueOffset, DefaultPairPtr ? DefaultPairPtr + MapLayout.ValueOffset : nullptr, Owner, InstanceGraph);
				}

				--Num;
			}
		}
	}
	else
	{
		for (int32 Index = 0, Num = MapHelper.Num(); Num; ++Index)
		{
			if (MapHelper.IsValidIndex(Index))
			{
				uint8* PairPtr = MapHelper.GetPairPtr(Index);

				if (bInstancedKey)
				{
					KeyProp->InstanceSubobjects(PairPtr, nullptr, Owner, InstanceGraph);
				}

				if (bInstancedValue)
				{
					ValueProp->InstanceSubobjects(PairPtr + MapLayout.ValueOffset, nullptr, Owner, InstanceGraph);
				}

				--Num;
			}
		}
	}
}

bool UMapProperty::SameType(const UProperty* Other) const
{
	auto* MapProp = (UMapProperty*)Other;
	return Super::SameType(Other) && KeyProp && ValueProp && KeyProp->SameType(MapProp->KeyProp) && ValueProp->SameType(MapProp->ValueProp);
}

/**
 * Checks to see if this property already has the supplied value as a key
 * @param	InMap			The address of the map
 * @param	InBaseAddress	The base address of the map
 * @param	InValue			The value to find in the map
 * @return True if InValue is a key in the map, false otherwise
 */
bool UMapProperty::HasKey(void* InMap, void* InBaseAddress, const FString& InValue) const
{
	FScriptMapHelper MapHelper(this, InMap);

	for (int32 Index = 0, ItemsLeft = MapHelper.Num(); ItemsLeft > 0; ++Index)
	{
		if (MapHelper.IsValidIndex(Index))
		{
			--ItemsLeft;

			uint8* PairPtr = MapHelper.GetPairPtr(Index);
			uint8* KeyPtr = KeyProp->ContainerPtrToValuePtr<uint8>(PairPtr);

			FString KeyValue;
			if ( KeyPtr != InBaseAddress && KeyProp->ExportText_Direct(KeyValue, KeyPtr, KeyPtr, nullptr, 0) )
			{
				if ( (Cast<UObjectProperty>(KeyProp) != nullptr && KeyValue.Contains(InValue)) || InValue == KeyValue )
				{
					return true;
				}
			}
		}
	}

	return false;
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UMapProperty, UProperty,
	{
		Class->EmitObjectReference(STRUCT_OFFSET(UMapProperty, KeyProp),   TEXT("KeyProp"));
		Class->EmitObjectReference(STRUCT_OFFSET(UMapProperty, ValueProp), TEXT("ValueProp"));

		// Ensure that TArray and FScriptMap are interchangeable, as FScriptMap will be used to access a native array property
		// from script that is declared as a TArray in C++.
		static_assert(sizeof(FScriptMap) == sizeof(TMap<uint32, uint8>), "FScriptMap and TMap<uint32, uint8> must be interchangable.");
	}
);

void FScriptMapHelper::Rehash()
{
	// Moved out-of-line to maybe fix a weird link error
	Map->Rehash(MapLayout, [=](const void* Src) {
		return KeyProp->GetValueTypeHash(Src);
	});
}
