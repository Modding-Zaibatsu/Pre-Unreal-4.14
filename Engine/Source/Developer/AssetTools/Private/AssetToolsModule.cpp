// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "AssetToolsModule.h"
#include "AssetToolsConsoleCommands.h"
#include "MessageLogModule.h"

IMPLEMENT_MODULE( FAssetToolsModule, AssetTools );
DEFINE_LOG_CATEGORY(LogAssetTools);

void FAssetToolsModule::StartupModule()
{
	AssetTools = new FAssetTools();
	ConsoleCommands = new FAssetToolsConsoleCommands(*this);

	// create a message log for the asset tools to use
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowPages = true;
	MessageLogModule.RegisterLogListing("AssetTools", NSLOCTEXT("AssetTools", "AssetToolsLogLabel", "Asset Tools"), InitOptions);
}

void FAssetToolsModule::ShutdownModule()
{
	if (AssetTools != NULL)
	{
		delete AssetTools;
		AssetTools = NULL;
	}

	if (ConsoleCommands != NULL)
	{
		delete ConsoleCommands;
		ConsoleCommands = NULL;
	}

	if (FModuleManager::Get().IsModuleLoaded("MessageLog"))
	{
		// unregister message log
		FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.UnregisterLogListing("AssetTools");
	}
}

IAssetTools& FAssetToolsModule::Get() const
{
	check(AssetTools);
	return *AssetTools;
}
