// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAmbient.h: Post processing ambient implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"
#include "AmbientCubemapParameters.h"

// ePId_Input0: SceneColor
// ePId_Input1: optional AmbientOcclusion
// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessAmbient : public TRenderingCompositePassBase<2, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual const TCHAR* GetDebugName() { return TEXT("FRCPassPostProcessAmbient"); }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual bool FrameBufferBlendingWithInput0() const { return true; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;

private:

	template<bool bUseClearCoat>
	void Render(FRenderingCompositePassContext& Context);
};
