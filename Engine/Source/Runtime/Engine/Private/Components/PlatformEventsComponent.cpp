// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Components/PlatformEventsComponent.h"
#include "CallbackDevice.h"


UPlatformEventsComponent::UPlatformEventsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }


bool UPlatformEventsComponent::IsInLaptopMode()
{
	return (FPlatformMisc::GetConvertibleLaptopMode() == EConvertibleLaptopMode::Laptop);
}


bool UPlatformEventsComponent::IsInTabletMode()
{
	return (FPlatformMisc::GetConvertibleLaptopMode() == EConvertibleLaptopMode::Tablet);
}


bool UPlatformEventsComponent::SupportsConvertibleLaptops()
{
	return (FPlatformMisc::GetConvertibleLaptopMode() != EConvertibleLaptopMode::NotSupported);
}


void UPlatformEventsComponent::OnRegister()
{
	Super::OnRegister();

	FCoreDelegates::PlatformChangedLaptopMode.AddUObject(this, &UPlatformEventsComponent::HandlePlatformChangedLaptopMode);
}


void UPlatformEventsComponent::OnUnregister()
{
	Super::OnUnregister();
	
 	FCoreDelegates::PlatformChangedLaptopMode.RemoveAll(this);
}


void UPlatformEventsComponent::HandlePlatformChangedLaptopMode(EConvertibleLaptopMode NewMode)
{
	if (NewMode == EConvertibleLaptopMode::Laptop)
	{
		PlatformChangedToLaptopModeDelegate.Broadcast();
	}
	else if (NewMode == EConvertibleLaptopMode::Tablet)
	{
		PlatformChangedToTabletModeDelegate.Broadcast();
	}
}
