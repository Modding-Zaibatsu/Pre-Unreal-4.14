// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWidgetReflector.h"

class FWidgetSnapshotService;

/**
 * Widget reflector implementation.
 * User widget to enable iteration without recompilation.
 */
class SLATEREFLECTOR_API SWidgetReflector
	: public SUserWidget
	, public IWidgetReflector
{
public:

	SLATE_USER_ARGS(SWidgetReflector)
	{ }
		
		SLATE_ARGUMENT(TSharedPtr<SDockTab>, ParentTab)
		SLATE_ARGUMENT(TSharedPtr<FWidgetSnapshotService>, WidgetSnapshotService)

	SLATE_END_ARGS()

	virtual void Construct( const FArguments& InArgs ) = 0;
};

