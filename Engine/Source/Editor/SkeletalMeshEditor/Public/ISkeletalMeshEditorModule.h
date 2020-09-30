// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "ISkeletalMeshEditor.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSkeletalMeshEditor, Log, All);

class ISkeletalMeshEditorModule : public IModuleInterface, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/** Creates a new skeleton editor instance */
	virtual TSharedRef<ISkeletalMeshEditor> CreateSkeletalMeshEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, class USkeletalMesh* InSkeletalMesh) = 0;

	/** Get all toolbar extenders */
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<FExtender>, FSkeletalMeshEditorToolbarExtender, const TSharedRef<FUICommandList> /*InCommandList*/, TSharedRef<ISkeletalMeshEditor> /*InSkeletalMeshEditor*/);
	virtual TArray<FSkeletalMeshEditorToolbarExtender>& GetAllSkeletalMeshEditorToolbarExtenders() = 0;
};
