// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/SModeWidget.h"

class FBlueprintEditor;

/**
 * Kismet menu
 */
class KISMET_API FKismet2Menu
{
public:
	static void SetupBlueprintEditorMenu( TSharedPtr< FExtender > Extender, FBlueprintEditor& Kismet);
	
protected:
	static void FillFileMenuBlueprintSection( FMenuBuilder& MenuBuilder, FBlueprintEditor& Kismet );

	static void FillEditMenu( FMenuBuilder& MenuBuilder );

	static void FillViewMenu( FMenuBuilder& MenuBuilder );

	static void FillDebugMenu( FMenuBuilder& MenuBuilder );

	static void FillDeveloperMenu( FMenuBuilder& MenuBuilder );

private:
	/** Diff current blueprint against the specified revision */
	static void DiffAgainstRevision( class UBlueprint* Current, int32 OldRevision );

	static TSharedRef<SWidget> MakeDiffMenu(FBlueprintEditor& Kismet);
};


class FFullBlueprintEditorCommands : public TCommands<FFullBlueprintEditorCommands>
{
public:
	/** Constructor */
	FFullBlueprintEditorCommands() 
		: TCommands<FFullBlueprintEditorCommands>("FullBlueprintEditor", NSLOCTEXT("Contexts", "FullBlueprintEditor", "Full Blueprint Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	/** Compile the blueprint */
	TSharedPtr<FUICommandInfo> Compile;
	TSharedPtr<FUICommandInfo> SaveOnCompile_Never;
	TSharedPtr<FUICommandInfo> SaveOnCompile_SuccessOnly;
	TSharedPtr<FUICommandInfo> SaveOnCompile_Always;
	TSharedPtr<FUICommandInfo> JumpToErrorNode;

	/** Profiler options  */
	TSharedPtr<FUICommandInfo> ToggleProfiler;

	/** Switch between modes in the blueprint editor */
	TSharedPtr<FUICommandInfo> SwitchToScriptingMode;
	TSharedPtr<FUICommandInfo> SwitchToBlueprintDefaultsMode;
	TSharedPtr<FUICommandInfo> SwitchToComponentsMode;
	
	/** Edit Blueprint global options */
	TSharedPtr<FUICommandInfo> EditGlobalOptions;
	TSharedPtr<FUICommandInfo> EditClassDefaults;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};



class KISMET_API FBlueprintEditorToolbar : public TSharedFromThis<FBlueprintEditorToolbar>
{
public:
	FBlueprintEditorToolbar(TSharedPtr<FBlueprintEditor> InBlueprintEditor)
		: BlueprintEditor(InBlueprintEditor) {}

	void AddBlueprintEditorModesToolbar(TSharedPtr<FExtender> Extender);
	void AddBlueprintGlobalOptionsToolbar(TSharedPtr<FExtender> Extender);
	void AddCompileToolbar(TSharedPtr<FExtender> Extender);
	void AddNewToolbar(TSharedPtr<FExtender> Extender);
	void AddScriptingToolbar(TSharedPtr<FExtender> Extender);
	void AddDebuggingToolbar(TSharedPtr<FExtender> Extender);
	void AddComponentsToolbar(TSharedPtr<FExtender> Extender);
	void AddProfilerToolbar(TSharedPtr<FExtender> Extender);

	/** Returns the current status icon for the blueprint being edited */
	FSlateIcon GetStatusImage() const;

	/** Returns the current profiler status tool tip */
	FText GetProfilerStatusTooltip() const;

	/** Returns the current status as text for the blueprint being edited */
	FText GetStatusTooltip() const;

	/** Helper function for generating the buttons in the toolbar, reused by merge and diff tools */
	static TArray< TSharedPtr< class SWidget> > GenerateToolbarWidgets(const class UBlueprint* BlueprintObj, TAttribute<FName> ActiveModeGetter, FOnModeChangeRequested ActiveModeSetter);

private:
	void FillBlueprintEditorModesToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillBlueprintGlobalOptionsToolbar(FToolBarBuilder& ToolBarBuilder);
	void FillCompileToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillNewToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillScriptingToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillDebuggingToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillComponentsToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillProfilerToolbar(FToolBarBuilder& ToolbarBuilder);

protected:
	/** Pointer back to the blueprint editor tool that owns us */
	TWeakPtr<FBlueprintEditor> BlueprintEditor;
};

