// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays a given static mesh
 */

#pragma once

#include "ThumbnailHelpers.h"
#include "BlueprintThumbnailRenderer.generated.h"

UCLASS(config=Editor,MinimalAPI)
class UBlueprintThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	virtual bool CanVisualizeAsset(UObject* Object) override;
	UNREALED_API virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas) override;
	// End UThumbnailRenderer Object

	// UObject implementation
	UNREALED_API virtual void BeginDestroy() override;
	// End UObject implementation

	/** Notifies the thumbnail scene to refresh components for the specified blueprint */
	void BlueprintChanged(class UBlueprint* Blueprint);

private:
	TClassInstanceThumbnailScene<FBlueprintThumbnailScene, 400> ThumbnailScenes;
};
