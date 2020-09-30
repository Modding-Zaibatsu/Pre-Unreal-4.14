// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimationRuntime.h"
#include "BoneControllers/AnimNode_WheelHandler.h"
#include "GameFramework/WheeledVehicle.h"
#include "Vehicles/WheeledVehicleMovementComponent.h"
#include "Vehicles/VehicleWheel.h"
#include "Animation/AnimInstanceProxy.h"


/////////////////////////////////////////////////////
// FAnimNode_WheelHandler

FAnimNode_WheelHandler::FAnimNode_WheelHandler()
{
	AnimInstanceProxy = nullptr;
}

void FAnimNode_WheelHandler::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += ")";

	DebugData.AddDebugItem(DebugLine);

	const TArray<FWheelAnimData>& WheelAnimData = AnimInstanceProxy->GetWheelAnimData();
	for (const FWheelLookupData& Wheel : Wheels)
	{
		if (Wheel.BoneReference.BoneIndex != INDEX_NONE)
		{
			DebugLine = FString::Printf(TEXT(" [Wheel Index : %d] Bone: %s , Rotation Offset : %s, Location Offset : %s"),
				Wheel.WheelIndex, *Wheel.BoneReference.BoneName.ToString(), *WheelAnimData[Wheel.WheelIndex].RotOffset.ToString(), *WheelAnimData[Wheel.WheelIndex].LocOffset.ToString());
		}
		else
		{
			DebugLine = FString::Printf(TEXT(" [Wheel Index : %d] Bone: %s (invalid bone)"),
				Wheel.WheelIndex, *Wheel.BoneReference.BoneName.ToString());
		}

		DebugData.AddDebugItem(DebugLine);
	}

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_WheelHandler::EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	const TArray<FWheelAnimData>& WheelAnimData = AnimInstanceProxy->GetWheelAnimData();

	const FBoneContainer& BoneContainer = MeshBases.GetPose().GetBoneContainer();
	for(const FWheelLookupData& Wheel : Wheels)
	{
		if (Wheel.BoneReference.IsValid(BoneContainer))
		{
			FCompactPoseBoneIndex WheelSimBoneIndex = Wheel.BoneReference.GetCompactPoseIndex(BoneContainer);

			// the way we apply transform is same as FMatrix or FTransform
			// we apply scale first, and rotation, and translation
			// if you'd like to translate first, you'll need two nodes that first node does translate and second nodes to rotate. 
			FTransform NewBoneTM = MeshBases.GetComponentSpaceTransform(WheelSimBoneIndex);

			FAnimationRuntime::ConvertCSTransformToBoneSpace(SkelComp, MeshBases, NewBoneTM, WheelSimBoneIndex, BCS_ComponentSpace);

			// Apply rotation offset
			const FQuat BoneQuat(WheelAnimData[Wheel.WheelIndex].RotOffset);
			NewBoneTM.SetRotation(BoneQuat * NewBoneTM.GetRotation());

			// Apply loc offset
			NewBoneTM.AddToTranslation(WheelAnimData[Wheel.WheelIndex].LocOffset);

			// Convert back to Component Space.
			FAnimationRuntime::ConvertBoneSpaceTransformToCS(SkelComp, MeshBases, NewBoneTM, WheelSimBoneIndex, BCS_ComponentSpace);

			// add back to it
			OutBoneTransforms.Add(FBoneTransform(WheelSimBoneIndex, NewBoneTM));
		}
	}
}

bool FAnimNode_WheelHandler::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) 
{
	// if both bones are valid
	for (const FWheelLookupData& Wheel : Wheels)
	{
		// if one of them is valid
		if (Wheel.BoneReference.IsValid(RequiredBones) == true)
		{
			return true;
		}
	}

	return false;
}

void FAnimNode_WheelHandler::InitializeBoneReferences(const FBoneContainer& RequiredBones) 
{
	const TArray<FWheelAnimData>& WheelAnimData = AnimInstanceProxy->GetWheelAnimData();
	const int32 NumWheels = WheelAnimData.Num();
	Wheels.Empty(NumWheels);

	for (int32 WheelIndex = 0; WheelIndex < NumWheels; ++WheelIndex)
	{
		FWheelLookupData* Wheel = new(Wheels)FWheelLookupData();
		Wheel->WheelIndex = WheelIndex;
		Wheel->BoneReference.BoneName = WheelAnimData[WheelIndex].BoneName;
		Wheel->BoneReference.Initialize(RequiredBones);
	}

	// sort by bone indices
	Wheels.Sort([](const FWheelLookupData& L, const FWheelLookupData& R) { return L.BoneReference.BoneIndex < R.BoneReference.BoneIndex; });
}

void FAnimNode_WheelHandler::Initialize(const FAnimationInitializeContext& Context)
{
	AnimInstanceProxy = (FVehicleAnimInstanceProxy*)Context.AnimInstanceProxy;	//TODO: This is cached for now because we need it in eval bone transforms.
}