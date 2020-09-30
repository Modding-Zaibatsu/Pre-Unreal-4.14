// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "SequenceRecorderCommands.h"

#define LOCTEXT_NAMESPACE "SequenceRecorderCommands"

FSequenceRecorderCommands::FSequenceRecorderCommands()
	: TCommands<FSequenceRecorderCommands>("SequenceRecorder.Common", LOCTEXT("Common", "Common"), NAME_None, FEditorStyle::GetStyleSetName())
{
}

void FSequenceRecorderCommands::RegisterCommands()
{
	UI_COMMAND(RecordAll, "Record", "Record all recordings", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::R));
	UI_COMMAND(StopAll, "StopAll", "Stop all recordings", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::S));
	UI_COMMAND(AddRecording, "Add", "Add a new recording", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveRecording, "Remove", "Remove selected recording", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE