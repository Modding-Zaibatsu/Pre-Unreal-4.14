// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateCore.h"
#include "ButtonStyleAsset.generated.h"

/**
 * An asset describing a button's appearance.
 * Just a wrapper for the struct with real data in it.style factory
 */
UCLASS(hidecategories=Object, MinimalAPI)
class UButtonStyleAsset : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** The actual data describing the button's appearance. */
	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FButtonStyle ButtonStyle;
};
