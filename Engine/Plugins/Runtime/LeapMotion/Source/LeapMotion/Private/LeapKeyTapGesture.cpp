// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LeapMotionPrivatePCH.h"
#include "LeapKeyTapGesture.h"

class FPrivateKeyTapGesture
{
public:
	Leap::KeyTapGesture Gesture;
};

ULeapKeyTapGesture::ULeapKeyTapGesture(const FObjectInitializer &ObjectInitializer) : ULeapGesture(ObjectInitializer), Private(new FPrivateKeyTapGesture())
{
}

ULeapKeyTapGesture::~ULeapKeyTapGesture()
{
	delete Private;
}

ULeapPointable* ULeapKeyTapGesture::Pointable()
{
	if (PPointable == nullptr)
	{
		PPointable = NewObject<ULeapPointable>(this);
	}
	PPointable->SetPointable(Private->Gesture.pointable());
	return (PPointable);
}

void ULeapKeyTapGesture::SetGesture(const Leap::KeyTapGesture &Gesture)
{
	//Super
	ULeapGesture::SetGesture(Gesture);

	Private->Gesture = Gesture;

	Direction = ConvertLeapToUE(Private->Gesture.direction());
	Position = ConvertAndScaleLeapToUE(Private->Gesture.position());
	Progress = Private->Gesture.progress();

	//Convenience
	BasicDirection = LeapBasicVectorDirection(Direction);
}