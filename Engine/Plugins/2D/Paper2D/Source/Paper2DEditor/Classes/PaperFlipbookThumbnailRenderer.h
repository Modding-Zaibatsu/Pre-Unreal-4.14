// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperSpriteThumbnailRenderer.h"
#include "PaperFlipbookThumbnailRenderer.generated.h"

UCLASS()
class UPaperFlipbookThumbnailRenderer : public UPaperSpriteThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// UThumbnailRenderer interface
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End of UThumbnailRenderer interface
};
