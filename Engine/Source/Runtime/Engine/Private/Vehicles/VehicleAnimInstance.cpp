// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UVehicleAnimInstance.cpp: Single Node Tree Instance 
	Only plays one animation at a time. 
=============================================================================*/ 

#include "EnginePrivate.h"
#include "Vehicles/VehicleAnimInstance.h"
#include "Vehicles/WheeledVehicleMovementComponent.h"
#include "Vehicles/VehicleWheel.h"
#include "GameFramework/WheeledVehicle.h"
#include "AnimationRuntime.h"

/////////////////////////////////////////////////////
// UVehicleAnimInstance
/////////////////////////////////////////////////////

UVehicleAnimInstance::UVehicleAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

class AWheeledVehicle * UVehicleAnimInstance::GetVehicle()
{
	return Cast<AWheeledVehicle> (GetOwningActor());
}

FAnimInstanceProxy* UVehicleAnimInstance::CreateAnimInstanceProxy()
{
	return &AnimInstanceProxy;
}

void UVehicleAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//// PROXY ///

void FVehicleAnimInstanceProxy::SetWheeledVehicleMovementComponent(const UWheeledVehicleMovementComponent* InWheeledVehicleMovementComponent)
{
	const UWheeledVehicleMovementComponent* WheeledVehicleMovementComponent = InWheeledVehicleMovementComponent;

	//initialize wheel data
	const int32 NumOfwheels = WheeledVehicleMovementComponent->WheelSetups.Num();
	WheelInstances.Empty(NumOfwheels);
	if (NumOfwheels > 0)
	{
		WheelInstances.AddZeroed(NumOfwheels);
		// now add wheel data
		for (int32 WheelIndex = 0; WheelIndex<WheelInstances.Num(); ++WheelIndex)
		{
			FWheelAnimData& WheelInstance = WheelInstances[WheelIndex];
			const FWheelSetup& WheelSetup = WheeledVehicleMovementComponent->WheelSetups[WheelIndex];

			// set data
			WheelInstance.BoneName = WheelSetup.BoneName;
			WheelInstance.LocOffset = FVector::ZeroVector;
			WheelInstance.RotOffset = FRotator::ZeroRotator;
		}
	}
}

void FVehicleAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	const UVehicleAnimInstance* VehicleAnimInstance = CastChecked<UVehicleAnimInstance>(InAnimInstance);
	if(const UWheeledVehicleMovementComponent* WheeledVehicleMovementComponent = VehicleAnimInstance->GetWheeledVehicleMovementComponent())
	{
		for (int32 WheelIndex = 0; WheelIndex < WheelInstances.Num(); ++ WheelIndex)
		{
			FWheelAnimData& WheelInstance = WheelInstances[WheelIndex];
			if (WheeledVehicleMovementComponent->Wheels.IsValidIndex(WheelIndex))
			{
				if (const UVehicleWheel* VehicleWheel = WheeledVehicleMovementComponent->Wheels[WheelIndex])
				{
					WheelInstance.RotOffset.Pitch = VehicleWheel->GetRotationAngle();
					WheelInstance.RotOffset.Yaw = VehicleWheel->GetSteerAngle();
					WheelInstance.RotOffset.Roll = 0.f;

					WheelInstance.LocOffset.X = 0.f;
					WheelInstance.LocOffset.Y = 0.f;
					WheelInstance.LocOffset.Z = VehicleWheel->GetSuspensionOffset();
				}
			}
		}
	}
}