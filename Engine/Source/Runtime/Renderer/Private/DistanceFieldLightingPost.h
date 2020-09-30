// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldLightingPost.h
=============================================================================*/

#pragma once

extern void AllocateOrReuseAORenderTarget(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget>& Target, const TCHAR* Name, bool bWithAlphaOrFP16Precision);

extern void UpdateHistory(
	FRHICommandList& RHICmdList,
	const FViewInfo& View, 
	const TCHAR* BentNormalHistoryRTName,
	const TCHAR* IrradianceHistoryRTName,
	IPooledRenderTarget* VelocityTexture,
	FSceneRenderTargetItem& DistanceFieldNormal,
	/** Contains last frame's history, if non-NULL.  This will be updated with the new frame's history. */
	TRefCountPtr<IPooledRenderTarget>* BentNormalHistoryState,
	TRefCountPtr<IPooledRenderTarget>* IrradianceHistoryState,
	/** Source */
	TRefCountPtr<IPooledRenderTarget>& BentNormalSource, 
	TRefCountPtr<IPooledRenderTarget>& IrradianceSource, 
	/** Output of Temporal Reprojection for the next step in the pipeline. */
	TRefCountPtr<IPooledRenderTarget>& BentNormalHistoryOutput,
	TRefCountPtr<IPooledRenderTarget>& IrradianceHistoryOutput);

extern void PostProcessBentNormalAOSurfaceCache(
	FRHICommandList& RHICmdList, 
	const FDistanceFieldAOParameters& Parameters, 
	const FViewInfo& View, 
	IPooledRenderTarget* VelocityTexture,
	FSceneRenderTargetItem& BentNormalInterpolation, 
	IPooledRenderTarget* IrradianceInterpolation,
	FSceneRenderTargetItem& DistanceFieldNormal,
	TRefCountPtr<IPooledRenderTarget>& BentNormalOutput,
	TRefCountPtr<IPooledRenderTarget>& IrradianceOutput);

extern void UpsampleBentNormalAO(
	FRHICommandList& RHICmdList, 
	const TArray<FViewInfo>& Views, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldAOBentNormal, 
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldIrradiance,
	TRefCountPtr<IPooledRenderTarget>& DistanceFieldSpecularOcclusion,
	bool bModulateSceneColor,
	bool bVisualizeAmbientOcclusion,
	bool bVisualizeGlobalIllumination);