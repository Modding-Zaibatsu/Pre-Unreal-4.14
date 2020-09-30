// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/////////////////////////////////////////////////////
// FApplicationMode

class KISMET_API FApplicationMode : public TSharedFromThis<FApplicationMode>
{
protected:
	// The layout to use in this mode
	TSharedPtr<FTabManager::FLayout> TabLayout;

	// The list of tabs that can be spawned in this game mode.
	// Note: This list should be treated as readonly outside of the constructor.
	//FWorkflowAllowedTabSet AllowableTabs;

	// The internal name of this mode
	FName ModeName;

	//@TODO: For test suite use only
	FString UserLayoutString;
	
	/** The toolbar extension for this mode */
	TSharedPtr<FExtender> ToolbarExtender;

	/** The workspace menu category for this mode */
	TSharedPtr<FWorkspaceItem> WorkspaceMenuCategory;

public:
	FApplicationMode(FName InModeName);

	FApplicationMode(FName InModeName, FText(*GetLocalizedMode)(const FName));

	virtual ~FApplicationMode() {}

	void DeactivateMode(TSharedPtr<FTabManager> InTabManager);
	TSharedRef<FTabManager::FLayout> ActivateMode(TSharedPtr<FTabManager> InTabManager);
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) {}
	
	virtual void PreDeactivateMode() {}
	virtual void PostActivateMode() {}

	FName GetModeName() const
	{
		return ModeName;
	}

	TSharedPtr<FExtender> GetToolbarExtender() { return ToolbarExtender; }

	/** @return The the workspace category for this asset editor */
	TSharedRef<FWorkspaceItem> GetWorkspaceMenuCategory() { return WorkspaceMenuCategory.ToSharedRef(); }
};
