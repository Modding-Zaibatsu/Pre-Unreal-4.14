// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessWeightedSampleSum.h: Post processing weight sample sum implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"
#include "PostProcessing.h"

enum EFilterCombineMethod
{
	EFCM_Weighted,		// for Gaussian blur, e.g. bloom
	EFCM_MaxMagnitude	// useful for motion blur
};

// to trigger certain optimizations and to orient the filter kernel
enum EFilterShape
{
	EFS_Horiz,
	EFS_Vert
};


/**
 * ePId_Input0: main input texture (usually to blur)
 * ePId_Input1: optional additive input (usually half res bloom)
 * N samples are added together, each sample is weighted.
 * derives from TRenderingCompositePassBase<InputCount, OutputCount>
 */
class FRCPassPostProcessWeightedSampleSum : public TRenderingCompositePassBase<2, 1>
{
public:
	// constructor
	FRCPassPostProcessWeightedSampleSum(EFilterShape InFilterShape,
			EFilterCombineMethod InCombineMethod,
			float InSizeScale,
			const TCHAR* InDebugName = TEXT("WeightedSampleSum"),
			FLinearColor InAdditiveTintValue = FLinearColor::White);

	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual void Release() override { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;

	// retrieve runtime filter kernel properties.
	static float GetClampedKernelRadius(ERHIFeatureLevel::Type InFeatureLevel, float KernelRadius);
	static int GetIntegerKernelRadius(ERHIFeatureLevel::Type InFeatureLevel, float KernelRadius);

	// @param InCrossCenterWeight >=0
	void SetCrossCenterWeight(float InCrossCenterWeight) { check(InCrossCenterWeight >= 0.0f); CrossCenterWeight = InCrossCenterWeight; }

private:
	void AdjustRectsForFastBlur(FIntRect& SrcRect, FIntRect& DestRect) const;
	void DrawClear(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, bool bDoFastBlur, FIntRect SrcRect, FIntRect DestRect, FIntPoint DestSize) const;
	void DrawQuad(FRHICommandListImmediate& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, bool bDoFastBlur, FIntRect SrcRect, FIntRect DestRect, FIntPoint DestSize, FIntPoint SrcSize, FShader* VertexShader) const;
	static uint32 GetMaxNumSamples(ERHIFeatureLevel::Type InFeatureLevel);

	// e.g. EFS_Horiz or EFS_Vert
	EFilterShape FilterShape;
	EFilterCombineMethod CombineMethod;
	float SizeScale;
	FLinearColor TintValue;
	const TCHAR* DebugName;
	// to give the center sample some special weight (see r.Bloom.Cross), >=0
	float CrossCenterWeight;

	// @return true: half x resolution for horizontal pass, vertical pass takes that as input, lower quality
	bool DoFastBlur() const;
};
