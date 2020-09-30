// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
*
* This thumbnail renderer displays a given GeometryCache
*/

#pragma once

#include "GeometryCacheEdModulePublicPCH.h"
#include "Classes/ThumbnailRendering/ThumbnailRenderer.h"
#include "Classes/ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "GeometryCacheThumbnailRenderer.generated.h"


class FGeometryCacheThumbnailScene;

UCLASS(config = Editor, MinimalAPI)
class UGeometryCacheThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object

	// Begin UObject implementation
	virtual void BeginDestroy() override;
	// End UObject implementation
private:
	FGeometryCacheThumbnailScene* ThumbnailScene;
};
