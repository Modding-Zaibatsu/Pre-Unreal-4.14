// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSkeletalMeshEditorCommands : public TCommands<FSkeletalMeshEditorCommands>
{
public:
	FSkeletalMeshEditorCommands()
		: TCommands<FSkeletalMeshEditorCommands>(TEXT("SkeletalMeshEditor"), NSLOCTEXT("Contexts", "SkeletalMeshEditor", "Skeletal Mesh Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:

	// reimport current mesh
	TSharedPtr<FUICommandInfo> ReimportMesh;
};
