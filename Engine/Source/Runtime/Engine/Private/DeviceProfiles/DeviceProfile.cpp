// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "DeviceProfiles/DeviceProfile.h"

UDeviceProfile::UDeviceProfile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BaseProfileName = TEXT("");
	DeviceType = TEXT("");

	bVisible = true;

	FString DeviceProfileFileName = FPaths::EngineConfigDir() + TEXT("Deviceprofiles.ini");
//	LoadConfig(GetClass(), *DeviceProfileFileName, UE4::LCPF_ReadParentSections);
}


void UDeviceProfile::GatherParentCVarInformationRecursively(OUT TMap<FString, FString>& CVarInformation) const
{
	// Recursively build the parent tree
	if (BaseProfileName != TEXT(""))
	{
		UDeviceProfile* ParentProfile = FindObject<UDeviceProfile>(GetTransientPackage(), *BaseProfileName);
		check(ParentProfile != NULL);

		for (auto& CurrentCVar : ParentProfile->CVars)
		{
			FString CVarKey, CVarValue;
			if (CurrentCVar.Split(TEXT("="), &CVarKey, &CVarValue))
			{
				if (CVarInformation.Find(CVarKey) == NULL)
				{
					CVarInformation.Add(CVarKey, *CurrentCVar);
				}
			}
		}

		ParentProfile->GatherParentCVarInformationRecursively(CVarInformation);
	}
}

UTextureLODSettings* UDeviceProfile::GetTextureLODSettings() const
{
	return (UTextureLODSettings*)this;
}


void UDeviceProfile::PostInitProperties()
{
	Super::PostInitProperties();

	// Ensure the Texture LOD Groups are in order of TextureGroup Enum
	TextureLODGroups.Sort([]
		(const FTextureLODGroup& Lhs, const FTextureLODGroup& Rhs)
		{
			return (int32)Lhs.Group < (int32)Rhs.Group;
		}
	);

	// Make sure every Texture Group has an entry, any that aren't specified for this profile should use it's parents values, or the defaults.
	UDeviceProfile* ParentProfile = nullptr;
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		if (BaseProfileName.IsEmpty() == false)
		{
			ParentProfile = FindObject<UDeviceProfile>(GetTransientPackage(), *BaseProfileName);
		}
		if (ParentProfile == nullptr)
		{
			ParentProfile = CastChecked<UDeviceProfile>(UDeviceProfile::StaticClass()->GetDefaultObject());
		}
	}

	for (int32 GroupId = 0; GroupId < (int32)TEXTUREGROUP_MAX; ++GroupId)
	{
		if (TextureLODGroups.Num() < (GroupId + 1) || TextureLODGroups[GroupId].Group > GroupId)
		{
			TextureLODGroups.Insert((ParentProfile ? ParentProfile->TextureLODGroups[GroupId] : FTextureLODGroup()), GroupId);
			TextureLODGroups[GroupId].Group = (TextureGroup)GroupId;
		}
	}

#define SETUPLODGROUP(GroupId) SetupLODGroup(GroupId);
	FOREACH_ENUM_TEXTUREGROUP(SETUPLODGROUP)
#undef SETUPLODGROUP
}

#if WITH_EDITOR

void UDeviceProfile::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty( PropertyChangedEvent );

	if( PropertyChangedEvent.Property->GetFName() == TEXT("BaseProfileName") )
	{
		FString NewParentName = *PropertyChangedEvent.Property->ContainerPtrToValuePtr<FString>( this );

		if( UObject* ParentRef = FindObject<UDeviceProfile>( GetTransientPackage(), *NewParentName ) )
		{
			// Generation and profile reference
			TMap<UDeviceProfile*,int32> DependentProfiles;

			int32 NumGenerations = 1;
			DependentProfiles.Add(this,0);

			for( TObjectIterator<UDeviceProfile> DeviceProfileIt; DeviceProfileIt; ++DeviceProfileIt )
			{
				UDeviceProfile* ParentProfile = *DeviceProfileIt;

				if( !ParentProfile->IsPendingKill() )
				{
					int32 ProfileGeneration = 1;
					do
					{
						if( this->GetName() == ParentProfile->BaseProfileName )
						{
							NumGenerations = NumGenerations > ProfileGeneration ? NumGenerations : ProfileGeneration;
							DependentProfiles.Add(*DeviceProfileIt,ProfileGeneration);
							break;
						}

						ParentProfile = FindObject<UDeviceProfile>( GetTransientPackage(), *ParentProfile->BaseProfileName );
						++ProfileGeneration;
					} while ( ParentProfile );
				}
			}


			UDeviceProfile* ClassCDO = CastChecked<UDeviceProfile>(GetClass()->GetDefaultObject());

			for( int32 CurrentGeneration = 0; CurrentGeneration < NumGenerations; CurrentGeneration++ )
			{
				for( TMap<UDeviceProfile*,int32>::TIterator DeviceProfileIt(DependentProfiles); DeviceProfileIt; ++DeviceProfileIt )
				{
					if( CurrentGeneration == DeviceProfileIt.Value() )
					{
						UDeviceProfile* CurrentGenerationProfile = DeviceProfileIt.Key();
						UDeviceProfile* ParentProfile = FindObject<UDeviceProfile>( GetTransientPackage(), *CurrentGenerationProfile->BaseProfileName );
						if( ParentProfile == NULL )
						{
							ParentProfile = ClassCDO;
						}

						for (TFieldIterator<UProperty> CurrentObjPropertyIter( GetClass() ); CurrentObjPropertyIter; ++CurrentObjPropertyIter)
						{
							bool bIsSameParent = CurrentObjPropertyIter->Identical_InContainer( ClassCDO, CurrentGenerationProfile );
							if( bIsSameParent )
							{
								void* CurrentGenerationProfilePropertyAddress = CurrentObjPropertyIter->ContainerPtrToValuePtr<void>( CurrentGenerationProfile );
								void* ParentPropertyAddr = CurrentObjPropertyIter->ContainerPtrToValuePtr<void>( ParentRef );

								CurrentObjPropertyIter->CopyCompleteValue( CurrentGenerationProfilePropertyAddress, ParentPropertyAddr );
							}
						}
					}
				}
			}
		}
		OnCVarsUpdated().ExecuteIfBound();
	}
	else if(PropertyChangedEvent.Property->GetFName() == TEXT("CVars"))
	{
		OnCVarsUpdated().ExecuteIfBound();
	}
}

#endif