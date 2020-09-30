// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/TextureRenderTarget.h"
#include "AssetTypeActions_Texture.h"

class FAssetTypeActions_TextureRenderTarget : public FAssetTypeActions_Texture
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureRenderTarget", "Texture Render Target"); }
	virtual FColor GetTypeColor() const override { return FColor(128,64,64); }
	virtual UClass* GetSupportedClass() const override { return UTextureRenderTarget::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual bool IsImportedAsset() const override { return false; }

private:
	/** Handler for when CreateStatic is selected */
	void ExecuteCreateStatic(TArray<TWeakObjectPtr<UTextureRenderTarget>> Objects);
};