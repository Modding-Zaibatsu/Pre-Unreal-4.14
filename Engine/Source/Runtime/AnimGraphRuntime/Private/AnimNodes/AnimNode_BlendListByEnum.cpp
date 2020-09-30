// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimationRuntime.h"
#include "AnimNodes/AnimNode_BlendListByEnum.h"

/////////////////////////////////////////////////////
// FAnimNode_BlendListByEnum

int32 FAnimNode_BlendListByEnum::GetActiveChildIndex()
{
	if (EnumToPoseIndex.IsValidIndex(ActiveEnumValue))
	{
		return EnumToPoseIndex[ActiveEnumValue];
	}
	else
	{
		return 0;
	}
}

