// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LandscapePrivatePCH.h"
#include "LandscapeInfo.h"

LANDSCAPE_API FLandscapeInfoLayerSettings::FLandscapeInfoLayerSettings(ULandscapeLayerInfoObject* InLayerInfo, class ALandscapeProxy* InProxy)
	: LayerInfoObj(InLayerInfo)
	, LayerName((InLayerInfo != NULL) ? InLayerInfo->LayerName : NAME_None)
#if WITH_EDITORONLY_DATA
	, ThumbnailMIC(NULL)
	, Owner(InProxy)
	, DebugColorChannel(0)
	, bValid(false)
#endif
{
}
