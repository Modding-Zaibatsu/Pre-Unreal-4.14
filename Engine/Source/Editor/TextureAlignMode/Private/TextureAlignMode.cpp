// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TextureAlignModePrivatePCH.h"

#define LOCTEXT_NAMESPACE "TextureAlignMode"

void FTextureAlignMode::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{

}

void FTextureAlignMode::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{

}

FName FTextureAlignMode::GetToolkitFName() const
{
	return FName("TextureAlignMode");
}

FText FTextureAlignMode::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Texture Align Mode");
}

class FEdMode* FTextureAlignMode::GetEditorMode() const
{
	return (FEdModeTexture*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Texture);
}

#undef LOCTEXT_NAMESPACE