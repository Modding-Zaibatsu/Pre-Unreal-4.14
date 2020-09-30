// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "IAnimationEditor.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAnimationEditor, Log, All);

class IAnimationEditorModule : public IModuleInterface, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/** Creates a new skeleton editor instance */
	virtual TSharedRef<IAnimationEditor> CreateAnimationEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, class UAnimationAsset* InAnimationAsset) = 0;

	/** Get all toolbar extenders */
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<FExtender>, FAnimationEditorToolbarExtender, const TSharedRef<FUICommandList> /*InCommandList*/, TSharedRef<IAnimationEditor> /*InAnimationEditor*/);
	virtual TArray<FAnimationEditorToolbarExtender>& GetAllAnimationEditorToolbarExtenders() = 0;
};
