// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimNodes/AnimNode_Root.h"

/////////////////////////////////////////////////////
// FAnimNode_Root

FAnimNode_Root::FAnimNode_Root()
{
}

void FAnimNode_Root::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize(Context);

	Result.Initialize(Context);
}

void FAnimNode_Root::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	Result.CacheBones(Context);
}

void FAnimNode_Root::Update(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);
	Result.Update(Context);
}

void FAnimNode_Root::Evaluate(FPoseContext& Output)
{
	Result.Evaluate(Output);
}

void FAnimNode_Root::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);
	Result.GatherDebugData(DebugData);
}