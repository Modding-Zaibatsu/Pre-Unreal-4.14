// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "SequenceRecorderBlueprintLibrary.h"
#include "SequenceRecorder.h"

void USequenceRecorderBlueprintLibrary::StartRecordingSequence(const TArray<AActor*>& ActorsToRecord)
{
	FSequenceRecorder::Get().ClearQueuedRecordings();
	for(AActor* Actor : ActorsToRecord)
	{
		FSequenceRecorder::Get().AddNewQueuedRecording(Actor);
	}
	
	FSequenceRecorder::Get().StartRecording();
}

bool USequenceRecorderBlueprintLibrary::IsRecordingSequence()
{
	return FSequenceRecorder::Get().IsRecording();
}

void USequenceRecorderBlueprintLibrary::StopRecordingSequence()
{
	FSequenceRecorder::Get().StopRecording();
}