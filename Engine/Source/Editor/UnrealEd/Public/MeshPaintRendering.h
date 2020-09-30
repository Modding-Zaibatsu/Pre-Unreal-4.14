// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	MeshPaintRendering.h: Mesh texture paint brush rendering
================================================================================*/

#ifndef __MeshPaintRendering_h__
#define __MeshPaintRendering_h__

#pragma once



namespace MeshPaintRendering
{
	/** Batched element parameters for mesh paint shaders */
	struct FMeshPaintShaderParameters
	{

	public:

		// @todo MeshPaint: Should be serialized no?
		UTextureRenderTarget2D* CloneTexture;

		FMatrix WorldToBrushMatrix;

		float BrushRadius;
		float BrushRadialFalloffRange;
		float BrushDepth;
		float BrushDepthFalloffRange;
		float BrushStrength;
		FLinearColor BrushColor;
		bool RedChannelFlag;
		bool BlueChannelFlag;
		bool GreenChannelFlag;
		bool AlphaChannelFlag;
		bool GenerateMaskFlag;


	};


	/** Batched element parameters for mesh paint dilation shaders used for seam painting */
	struct FMeshPaintDilateShaderParameters
	{

	public:

		UTextureRenderTarget2D* Texture0;
		UTextureRenderTarget2D* Texture1;
		UTextureRenderTarget2D* Texture2;

		float WidthPixelOffset;
		float HeightPixelOffset;

	};


	/** Binds the mesh paint vertex and pixel shaders to the graphics device */
	void UNREALED_API SetMeshPaintShaders(FRHICommandList& RHICmdList, 
											ERHIFeatureLevel::Type InFeatureLevel, 
											const FMatrix& InTransform,
											const float InGamma,
											const FMeshPaintShaderParameters& InShaderParams );

	/** Binds the mesh paint dilation vertex and pixel shaders to the graphics device */
	void UNREALED_API SetMeshPaintDilateShaders(FRHICommandList& RHICmdList, 
													ERHIFeatureLevel::Type InFeatureLevel, 
													const FMatrix& InTransform,
													const float InGamma,
													const FMeshPaintDilateShaderParameters& InShaderParams );

}



#endif	// __MeshPaintRendering_h__
