// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class IAnimationSequenceBrowser : public SCompoundWidget
{
public:
	/** Select the specified asset */
	virtual void SelectAsset(UAnimationAsset* InAsset) = 0;
	/** Add to navigation history */
	virtual void AddToHistory(UAnimationAsset * AnimAsset) = 0;
};