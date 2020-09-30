// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Class containing commands for pose editor
*/
class FPoseEditorCommands : public TCommands<FPoseEditorCommands>
{
public:
	FPoseEditorCommands()
	: TCommands<FPoseEditorCommands>
		(
			TEXT("PoseEditor"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "PoseEditor", "Pose Editor"), // Localized context name for displaying
			NAME_None, // Parent context name.  
			FEditorStyle::GetStyleSetName() // Icon Style Set
		)
	{}

	/** Initialize commands */
	virtual void RegisterCommands() override;

	/** Paste all names */
	TSharedPtr< FUICommandInfo > PasteAllNames;
};


