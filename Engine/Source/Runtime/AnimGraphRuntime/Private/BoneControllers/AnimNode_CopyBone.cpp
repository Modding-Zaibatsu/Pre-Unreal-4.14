// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "BoneControllers/AnimNode_CopyBone.h"

/////////////////////////////////////////////////////
// FAnimNode_CopyBone

FAnimNode_CopyBone::FAnimNode_CopyBone()
	: bCopyTranslation(false)
	, bCopyRotation(false)
	, bCopyScale(false)
	, ControlSpace(BCS_ComponentSpace)
{
}

void FAnimNode_CopyBone::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += FString::Printf(TEXT(" Src: %s Dst: %s)"), *SourceBone.BoneName.ToString(), *TargetBone.BoneName.ToString());
	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_CopyBone::EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	// Pass through if we're not doing anything.
	if( !bCopyTranslation && !bCopyRotation && !bCopyScale )
	{
		return;
	}

	// Get component space transform for source and current bone.
	const FBoneContainer& BoneContainer = MeshBases.GetPose().GetBoneContainer();
	FCompactPoseBoneIndex SourceBoneIndex = SourceBone.GetCompactPoseIndex(BoneContainer);
	FCompactPoseBoneIndex TargetBoneIndex = TargetBone.GetCompactPoseIndex(BoneContainer);

	FTransform SourceBoneTM = MeshBases.GetComponentSpaceTransform(SourceBoneIndex);
	FTransform CurrentBoneTM = MeshBases.GetComponentSpaceTransform(TargetBoneIndex);

	if(ControlSpace != BCS_ComponentSpace)
	{
		// Convert out to selected space
		FAnimationRuntime::ConvertCSTransformToBoneSpace(SkelComp, MeshBases, SourceBoneTM, SourceBoneIndex, ControlSpace);
		FAnimationRuntime::ConvertCSTransformToBoneSpace(SkelComp, MeshBases, CurrentBoneTM, TargetBoneIndex, ControlSpace);
	}
	
	// Copy individual components
	if (bCopyTranslation)
	{
		CurrentBoneTM.SetTranslation( SourceBoneTM.GetTranslation() );
	}

	if (bCopyRotation)
	{
		CurrentBoneTM.SetRotation( SourceBoneTM.GetRotation() );
	}

	if (bCopyScale)
	{
		CurrentBoneTM.SetScale3D( SourceBoneTM.GetScale3D() );
	}

	if(ControlSpace != BCS_ComponentSpace)
	{
		// Convert back out if we aren't operating in component space
		FAnimationRuntime::ConvertBoneSpaceTransformToCS(SkelComp, MeshBases, CurrentBoneTM, TargetBoneIndex, ControlSpace);
	}

	// Output new transform for current bone.
	OutBoneTransforms.Add(FBoneTransform(TargetBoneIndex, CurrentBoneTM));
}

bool FAnimNode_CopyBone::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) 
{
	// if both bones are valid
	return (TargetBone.IsValid(RequiredBones) && (TargetBone==SourceBone || SourceBone.IsValid(RequiredBones)));
}

void FAnimNode_CopyBone::InitializeBoneReferences(const FBoneContainer& RequiredBones) 
{
	SourceBone.Initialize(RequiredBones);
	TargetBone.Initialize(RequiredBones);
}