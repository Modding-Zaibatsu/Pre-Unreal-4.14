#pragma once
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/PreviewMeshCollection.h"
#include "IAssetTypeActions.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_PreviewMeshCollection : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PreviewMeshCollection", "Preview Mesh Collection"); }
	virtual FColor GetTypeColor() const override { return FColor(190, 20, 70); }
	virtual UClass* GetSupportedClass() const override { return UPreviewMeshCollection::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};