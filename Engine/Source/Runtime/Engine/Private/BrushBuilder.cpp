// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UBrushBuilder.cpp: UnrealEd brush builder.
=============================================================================*/

#include "EnginePrivate.h"
#include "Engine/BrushBuilder.h"

/*-----------------------------------------------------------------------------
	UBrushBuilder.
-----------------------------------------------------------------------------*/
UBrushBuilder::UBrushBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BitmapFilename = TEXT("BBGeneric");
	ToolTip = TEXT("BrushBuilderName_Generic");
	NotifyBadParams = true;
}