// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "GameFramework/TouchInterface.h"

UClass* FAssetTypeActions_TouchInterface::GetSupportedClass() const
{
	return UTouchInterface::StaticClass();
}
