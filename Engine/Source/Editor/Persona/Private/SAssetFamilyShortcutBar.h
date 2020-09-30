// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class SAssetFamilyShortcutBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetFamilyShortcutBar)
	{}
		
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IAssetFamily>& InAssetFamily);

private:
	/** The thumbnail pool for displaying asset shortcuts */
	TSharedPtr<class FAssetThumbnailPool> ThumbnailPool;
};