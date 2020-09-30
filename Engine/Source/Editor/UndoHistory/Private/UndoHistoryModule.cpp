// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UndoHistoryPrivatePCH.h"
#include "UndoHistoryModule.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "FUndoHistoryModule"

void FUndoHistoryModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UndoHistoryTabName, FOnSpawnTab::CreateRaw(this, &FUndoHistoryModule::HandleSpawnSettingsTab))
		.SetDisplayName(NSLOCTEXT("FUndoHistoryModule", "UndoHistoryTabTitle", "Undo History"))
		.SetTooltipText(NSLOCTEXT("FUndoHistoryModule", "UndoHistoryTooltipText", "Open the Undo History tab."))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "UndoHistory.TabIcon"))
		.SetAutoGenerateMenuEntry(false);
}

void FUndoHistoryModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UndoHistoryTabName);
}

bool FUndoHistoryModule::SupportsDynamicReloading()
{
	return true;
}

TSharedRef<SDockTab> FUndoHistoryModule::HandleSpawnSettingsTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	DockTab->SetContent(SNew(SUndoHistory));

	return DockTab;
}


IMPLEMENT_MODULE(FUndoHistoryModule, UndoHistory);


#undef LOCTEXT_NAMESPACE
