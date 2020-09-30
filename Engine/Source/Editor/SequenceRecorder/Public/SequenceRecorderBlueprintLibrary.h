// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequenceRecorderBlueprintLibrary.generated.h"

UCLASS()
class SEQUENCERECORDER_API USequenceRecorderBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** 
	 * Start recording the passed-in actors to a level sequence.
	 * @param	ActorsToRecord	The actors to record
	 */
	UFUNCTION(BlueprintCallable, Category="Sequence Recording")
	static void StartRecordingSequence(const TArray<AActor*>& ActorsToRecord);

	/** Are we currently recording a sequence */
	UFUNCTION(BlueprintPure, Category="Sequence Recording")
	static bool IsRecordingSequence();

	/** Stop recording the current sequence, if any */
	UFUNCTION(BlueprintCallable, Category="Sequence Recording")
	static void StopRecordingSequence();
};