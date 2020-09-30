// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LeapMotionPrivatePCH.h"
#include "LeapTool.h"

class FPrivateTool
{
public:
	Leap::Tool Tool;
};

ULeapTool::ULeapTool(const FObjectInitializer &ObjectInitializer) : ULeapPointable(ObjectInitializer), Private(new FPrivateTool())
{
}

ULeapTool::~ULeapTool()
{
	delete Private;
}

void ULeapTool::SetTool(const Leap::Tool &Tool)
{
	Private->Tool = Tool;

	SetPointable(Private->Tool);
}