// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessLensBlur.cpp: Post processing lens blur implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessPassThrough.h"
#include "PostProcessing.h"
#include "PostProcessLensBlur.h"
#include "SceneUtils.h"

/** Encapsulates the post processing vertex shader. */
class FPostProcessLensBlurVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessLensBlurVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessLensBlurVS() {}
	
public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter TileCountAndSize;
	FShaderParameter KernelSize;
	FShaderParameter ColorScale;

	/** Initialization constructor. */
	FPostProcessLensBlurVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		TileCountAndSize.Bind(Initializer.ParameterMap, TEXT("TileCountAndSize"));
		KernelSize.Bind(Initializer.ParameterMap, TEXT("KernelSize"));
		ColorScale.Bind(Initializer.ParameterMap,TEXT("ColorScale"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << TileCountAndSize << KernelSize << ColorScale;
		return bShaderHasOutdatedParameters;
	}

	/** to have a similar interface as all other shaders */
	void SetParameters(const FRenderingCompositePassContext& Context, FIntPoint TileCountValue, uint32 TileSize, float PixelKernelSize, float Threshold)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		{
			FIntRect TileCountAndSizeValue(TileCountValue, FIntPoint(TileSize, TileSize));

			SetShaderValue(Context.RHICmdList, ShaderRHI, TileCountAndSize, TileCountAndSizeValue);
		}

		{
			// only approximate as the mip mapping doesn't produce accurate brightness scaling
			FVector4 ColorScaleValue(1.0f / FMath::Max(1.0f, PixelKernelSize * PixelKernelSize), Threshold, 0, 0);

			SetShaderValue(Context.RHICmdList, ShaderRHI, ColorScale, ColorScaleValue);
		}

		{
			FVector4 KernelSizeValue(PixelKernelSize, PixelKernelSize, 0, 0);

			SetShaderValue(Context.RHICmdList, ShaderRHI, KernelSize, KernelSizeValue);
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessLensBlurVS,TEXT("PostProcessLensBlur"),TEXT("MainVS"),SF_Vertex);

/** Encapsulates a simple copy pixel shader. */
class FPostProcessLensBlurPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessLensBlurPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FPostProcessLensBlurPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter LensTexture;
	FShaderResourceParameter LensTextureSampler;

	/** Initialization constructor. */
	FPostProcessLensBlurPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		LensTexture.Bind(Initializer.ParameterMap,TEXT("LensTexture"));
		LensTextureSampler.Bind(Initializer.ParameterMap,TEXT("LensTextureSampler"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << LensTexture << LensTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FRenderingCompositePassContext& Context, float PixelKernelSize)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
				
		{
			FTextureRHIParamRef TextureRHI = GWhiteTexture->TextureRHI;

			if(GEngine->DefaultBokehTexture)
			{
				FTextureResource* BokehTextureResource = GEngine->DefaultBokehTexture->Resource;

				if(BokehTextureResource && BokehTextureResource->TextureRHI)
				{
					TextureRHI = BokehTextureResource->TextureRHI;
				}
			}

			if(Context.View.FinalPostProcessSettings.LensFlareBokehShape)
			{
				FTextureResource* BokehTextureResource = Context.View.FinalPostProcessSettings.LensFlareBokehShape->Resource;

				if(BokehTextureResource && BokehTextureResource->TextureRHI)
				{
					TextureRHI = BokehTextureResource->TextureRHI;
				}
			}

			SetTextureParameter(Context.RHICmdList, ShaderRHI, LensTexture, LensTextureSampler, TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Clamp>::GetRHI(), TextureRHI);
		}
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessLensBlurPS,TEXT("PostProcessLensBlur"),TEXT("MainPS"),SF_Pixel);


FRCPassPostProcessLensBlur::FRCPassPostProcessLensBlur(float InPercentKernelSize, float InThreshold)
	: PercentKernelSize(InPercentKernelSize), Threshold(InThreshold)
{
}

void FRCPassPostProcessLensBlur::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PassPostProcessLensBlur);

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	
	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;

	FIntPoint TexSize = InputDesc->Extent;

	// usually 1, 2, 4 or 8
	uint32 ScaleToFullRes = FSceneRenderTargets::Get(Context.RHICmdList).GetBufferSizeXY().X / TexSize.X;

	FIntRect ViewRect = FIntRect::DivideAndRoundUp(View.ViewRect, ScaleToFullRes);
	FIntPoint ViewSize = ViewRect.Size();

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorExistingDepth);

	Context.SetViewportAndCallRHI(ViewRect);

	// set the state (additive blending)
	Context.RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessLensBlurVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessLensBlurPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	
	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GEmptyVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	uint32 TileSize = 1;

	FIntPoint TileCount = ViewSize / TileSize;

	float PixelKernelSize = PercentKernelSize / 100.0f * ViewSize.X;

	VertexShader->SetParameters(Context, TileCount, TileSize, PixelKernelSize, Threshold);
	PixelShader->SetParameters(Context, PixelKernelSize);

	Context.RHICmdList.SetStreamSource(0, NULL, 0, 0);

	// needs to be the same on shader side (faster on NVIDIA and AMD)
	int32 QuadsPerInstance = 4;

	Context.RHICmdList.DrawPrimitive(PT_TriangleList, 0, 2, FMath::DivideAndRoundUp(TileCount.X * TileCount.Y, QuadsPerInstance));

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}


FPooledRenderTargetDesc FRCPassPostProcessLensBlur::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.ClearValue = FClearValueBinding::Black;

	// more precision for additive blending
	Ret.Format = PF_FloatRGBA;
	Ret.DebugName = TEXT("LensBlur");

	return Ret;
}
