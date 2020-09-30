// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/UserDefinedStruct.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_Struct : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Struct", "Structure"); }
	virtual FColor GetTypeColor() const override { return FColor(103, 206, 218); }
	virtual UClass* GetSupportedClass() const override { return UUserDefinedStruct::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Blueprint; }
	virtual bool CanLocalize() const override { return false; }

	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
};
