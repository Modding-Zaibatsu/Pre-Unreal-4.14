// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeviceProfile.h: Declares the UDeviceProfile class.
=============================================================================*/

#pragma once

#include "Engine/TextureLODSettings.h"
#include "DeviceProfile.generated.h"


DECLARE_DELEGATE(FOnCVarsUpdated);


UCLASS(config=DeviceProfiles, perObjectConfig)
class ENGINE_API UDeviceProfile : public UTextureLODSettings
{
	GENERATED_UCLASS_BODY()

	/** The type of this profile, I.e. IOS, Windows, PS4 etc */
	UPROPERTY(VisibleAnywhere, config, Category=DeviceSettings)
	FString DeviceType;

	/** The name of the parent profile of this object */
	UPROPERTY(EditAnywhere, config, Category=DeviceSettings)
	FString BaseProfileName;

	/** The parent object of this profile, it is the object matching this DeviceType with the BaseProfileName */
	UPROPERTY()
	UObject* Parent;

	/** Flag used in the editor to determine whether the profile is visible in the property matrix */
	bool bVisible;


public:

	/** The collection of CVars which is set from this profile */
	UPROPERTY(EditAnywhere, config, Category=ConsoleVariables)
	TArray<FString> CVars;

	/** 
	 * Get the collection of Console Variables that this profile inherits from its' parents
	 *
	 * @param CVarInformation - The list of inherited CVars in the format of<key:CVarName,value:CVarCompleteString>
	 */
	void GatherParentCVarInformationRecursively(OUT TMap<FString, FString>& CVarInformation) const;

	/** 
	 * Accessor to the delegate object fired when there has been any changes to the console variables 
	 */
	FOnCVarsUpdated& OnCVarsUpdated()
	{
		return CVarsUpdatedDelegate;
	}

public:
	/** 
	 * Access to the device profiles Texture LOD Settings
	 */
	UTextureLODSettings* GetTextureLODSettings() const;

private:

	/** Delegate object fired when there has been any changes to the console variables */
	FOnCVarsUpdated CVarsUpdatedDelegate;

public:

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent ) override;
	//~ End UObject Interface
#endif
};
