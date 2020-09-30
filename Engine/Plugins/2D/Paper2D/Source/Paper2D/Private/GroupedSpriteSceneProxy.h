// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperRenderSceneProxy.h"

class UPaperGroupedSpriteComponent;

//////////////////////////////////////////////////////////////////////////
// FGroupedSpriteSceneProxy

class FGroupedSpriteSceneProxy : public FPaperRenderSceneProxy
{
public:
	FGroupedSpriteSceneProxy(UPaperGroupedSpriteComponent* InComponent);

protected:
	// FPaperRenderSceneProxy interface
	virtual void DebugDrawCollision(const FSceneView* View, int32 ViewIndex, FMeshElementCollector& Collector, bool bDrawSolid) const override;
	// End of FPaperRenderSceneProxy interface

private:
	const UPaperGroupedSpriteComponent* MyComponent;

	/** Per instance render data, could be shared with component */
	TSharedPtr<FPerInstanceRenderData, ESPMode::ThreadSafe> PerInstanceRenderData;

	/** Number of instances */
	int32 NumInstances;

	TArray<FMatrix> BodySetupTransforms;
	TArray<TWeakObjectPtr<UBodySetup>> BodySetups;

private:
	FSpriteRenderSection& FindOrAddSection(FSpriteDrawCallRecord& InBatch, UMaterialInterface* InMaterial);
};
