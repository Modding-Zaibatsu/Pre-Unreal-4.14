// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays a given anim sequence
 */

#pragma once
#include "AnimSequenceThumbnailRenderer.generated.h"

UCLASS(config=Editor, MinimalAPI)
class UAnimSequenceThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()


	// Begin UThumbnailRenderer Object
	UNREALED_API virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object

	// UObject implementation
	UNREALED_API virtual void BeginDestroy() override;

private:
	class FAnimationSequenceThumbnailScene* ThumbnailScene;
};

