// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// Actions that can be invoked in the reference viewer
class FReferenceViewerActions : public TCommands<FReferenceViewerActions>
{
public:
	FReferenceViewerActions() : TCommands<FReferenceViewerActions>
	(
		"ReferenceViewer",
		NSLOCTEXT("Contexts", "ReferenceViewer", "Reference Viewer"),
		"MainFrame", FEditorStyle::GetStyleSetName()
	)
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface
public:

	// Opens the selected asset in the asset editor
	TSharedPtr<FUICommandInfo> OpenSelectedInAssetEditor;

	// Re-constructs the graph with the selected asset as the center
	TSharedPtr<FUICommandInfo> ReCenterGraph;

	// Shows a list of objects that the selected asset references.
	TSharedPtr<FUICommandInfo> ListReferencedObjects;

	// Lists objects that reference the selected asset.
	TSharedPtr<FUICommandInfo> ListObjectsThatReference;

	// Shows a size map for the selected asset.
	TSharedPtr<FUICommandInfo> ShowSizeMap;

	// Shows a reference tree for the selected asset.
	TSharedPtr<FUICommandInfo> ShowReferenceTree;

	// Creates a new collection with the list of assets that this asset references, user selects which ECollectionShareType to use.
	TSharedPtr<FUICommandInfo> MakeLocalCollectionWithReferencers;
	TSharedPtr<FUICommandInfo> MakePrivateCollectionWithReferencers;
	TSharedPtr<FUICommandInfo> MakeSharedCollectionWithReferencers;
	TSharedPtr<FUICommandInfo> MakeLocalCollectionWithDependencies;
	TSharedPtr<FUICommandInfo> MakePrivateCollectionWithDependencies;
	TSharedPtr<FUICommandInfo> MakeSharedCollectionWithDependencies;
};
