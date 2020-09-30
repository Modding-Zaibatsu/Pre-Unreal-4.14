// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "SGraphPalette.h"

//////////////////////////////////////////////////////////////////////////

class SSoundCuePalette : public SGraphPalette
{
public:
	SLATE_BEGIN_ARGS( SSoundCuePalette ) {};
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	/** Callback used to populate all actions list in SGraphActionMenu */
	virtual void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) override;
};
