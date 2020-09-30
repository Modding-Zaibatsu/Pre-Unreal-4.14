// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FogRendering.cpp: Fog rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"

DECLARE_FLOAT_COUNTER_STAT(TEXT("Fog"), Stat_GPU_Fog, STATGROUP_GPU);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<float> CVarFogStartDistance(
	TEXT("r.FogStartDistance"),
	-1.0f,
	TEXT("Allows to override the FogStartDistance setting (needs ExponentialFog in the level).\n")
	TEXT(" <0: use default settings (default: -1)\n")
	TEXT(">=0: override settings by the given value (in world units)"),
	ECVF_Cheat | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarFogDensity(
	TEXT("r.FogDensity"),
	-1.0f,
	TEXT("Allows to override the FogDensity setting (needs ExponentialFog in the level).\n")
	TEXT("Using a strong value allows to quickly see which pixel are affected by fog.\n")
	TEXT("Using a start distance allows to cull pixels are can speed up rendering.\n")
	TEXT(" <0: use default settings (default: -1)\n")
	TEXT(">=0: override settings by the given value (0:off, 1=very dense fog)"),
	ECVF_Cheat | ECVF_RenderThreadSafe);
#endif

static TAutoConsoleVariable<int32> CVarFog(
	TEXT("r.Fog"),
	1,
	TEXT(" 0: disabled\n")
	TEXT(" 1: enabled (default)"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

/** Binds the parameters. */
void FExponentialHeightFogShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	ExponentialFogParameters.Bind(ParameterMap,TEXT("ExponentialFogParameters"));
	ExponentialFogColorParameter.Bind(ParameterMap,TEXT("ExponentialFogColorParameter"));
	ExponentialFogParameters3.Bind(ParameterMap,TEXT("ExponentialFogParameters3"));
	InscatteringLightDirection.Bind(ParameterMap,TEXT("InscatteringLightDirection"));
	DirectionalInscatteringColor.Bind(ParameterMap,TEXT("DirectionalInscatteringColor"));
	DirectionalInscatteringStartDistance.Bind(ParameterMap,TEXT("DirectionalInscatteringStartDistance"));
}

/** Serializer. */
FArchive& operator<<(FArchive& Ar,FExponentialHeightFogShaderParameters& Parameters)
{
	Ar << Parameters.ExponentialFogParameters;
	Ar << Parameters.ExponentialFogColorParameter;
	Ar << Parameters.ExponentialFogParameters3;
	Ar << Parameters.InscatteringLightDirection;
	Ar << Parameters.DirectionalInscatteringColor;
	Ar << Parameters.DirectionalInscatteringStartDistance;
	return Ar;
}

/** Binds the parameters. */
void FHeightFogShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	ExponentialParameters.Bind(ParameterMap);
}

/** Serializer. */
FArchive& operator<<(FArchive& Ar,FHeightFogShaderParameters& Parameters)
{
	Ar << Parameters.ExponentialParameters;
	return Ar;
}


/** A vertex shader for rendering height fog. */
class FHeightFogVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FHeightFogVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FHeightFogVS( )	{ }
	FHeightFogVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		FogStartZ.Bind(Initializer.ParameterMap,TEXT("FogStartZ"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		FGlobalShader::SetParameters(RHICmdList, GetVertexShader(),View);

		{
			// The fog can be set to start at a certain euclidean distance.
			// clamp the value to be behind the near plane z
			float FogStartDistance = FMath::Max(30.0f, View.ExponentialFogParameters.W);

			// Here we compute the nearest z value the fog can start
			// to render the quad at this z value with depth test enabled.
			// This means with a bigger distance specified more pixels are
			// are culled and don't need to be rendered. This is faster if
			// there is opaque content nearer than the computed z.

			FMatrix InvProjectionMatrix = View.ViewMatrices.GetInvProjMatrix();

			FVector ViewSpaceCorner = InvProjectionMatrix.TransformFVector4(FVector4(1, 1, 1, 1));

			float Ratio = ViewSpaceCorner.Z / ViewSpaceCorner.Size();

			FVector ViewSpaceStartFogPoint(0.0f, 0.0f, FogStartDistance * Ratio);
			FVector4 ClipSpaceMaxDistance = View.ViewMatrices.ProjMatrix.TransformPosition(ViewSpaceStartFogPoint);

			float FogClipSpaceZ = ClipSpaceMaxDistance.Z / ClipSpaceMaxDistance.W;

			SetShaderValue(RHICmdList, GetVertexShader(),FogStartZ, FogClipSpaceZ);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FogStartZ;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter FogStartZ;
};

IMPLEMENT_SHADER_TYPE(,FHeightFogVS,TEXT("HeightFogVertexShader"),TEXT("Main"),SF_Vertex);

/** A pixel shader for rendering exponential height fog. */
class FExponentialHeightFogPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FExponentialHeightFogPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FExponentialHeightFogPS( )	{ }
	FExponentialHeightFogPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		ExponentialParameters.Bind(Initializer.ParameterMap);
		OcclusionTexture.Bind(Initializer.ParameterMap, TEXT("OcclusionTexture"));
		OcclusionSampler.Bind(Initializer.ParameterMap, TEXT("OcclusionSampler"));
		SceneTextureParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FLightShaftsOutput& LightShaftsOutput)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);
		SceneTextureParameters.Set(RHICmdList, GetPixelShader(), View);
		ExponentialParameters.Set(RHICmdList, GetPixelShader(), &View);

		FTextureRHIRef TextureRHI = LightShaftsOutput.LightShaftOcclusion ?
			LightShaftsOutput.LightShaftOcclusion->GetRenderTargetItem().ShaderResourceTexture :
			GWhiteTexture->TextureRHI;

		SetTextureParameter(
			RHICmdList, 
			GetPixelShader(),
			OcclusionTexture, OcclusionSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			TextureRHI
			);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters;
		Ar << OcclusionTexture;
		Ar << OcclusionSampler;
		Ar << ExponentialParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderResourceParameter OcclusionTexture;
	FShaderResourceParameter OcclusionSampler;
	FExponentialHeightFogShaderParameters ExponentialParameters;
};

IMPLEMENT_SHADER_TYPE(,FExponentialHeightFogPS,TEXT("HeightFogPixelShader"), TEXT("ExponentialPixelMain"),SF_Pixel)

/** The fog vertex declaration resource type. */
class FFogVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor
	virtual ~FFogVertexDeclaration() {}

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float2, 0, sizeof(FVector2D)));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** Vertex declaration for the light function fullscreen 2D quad. */
TGlobalResource<FFogVertexDeclaration> GFogVertexDeclaration;

void FSceneRenderer::InitFogConstants()
{
	// console command override
	float FogDensityOverride = -1.0f;
	float FogStartDistanceOverride = -1.0f;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		// console variable overrides
		FogDensityOverride = CVarFogDensity.GetValueOnAnyThread();
		FogStartDistanceOverride = CVarFogStartDistance.GetValueOnAnyThread();
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		InitAtmosphereConstantsInView(View);
		// set fog consts based on height fog components
		if(ShouldRenderFog(*View.Family))
		{
			if (Scene->ExponentialFogs.Num() > 0)
			{
				const FExponentialHeightFogSceneInfo& FogInfo = Scene->ExponentialFogs[0];
				const float CosTerminatorAngle = FMath::Clamp(FMath::Cos(FogInfo.LightTerminatorAngle * PI / 180.0f), -1.0f + DELTA, 1.0f - DELTA);
				const float CollapsedFogParameterPower = FMath::Clamp(
						-FogInfo.FogHeightFalloff * (View.ViewMatrices.ViewOrigin.Z - FogInfo.FogHeight),
						-126.f + 1.f, // min and max exponent values for IEEE floating points (http://en.wikipedia.org/wiki/IEEE_floating_point)
						+127.f - 1.f
						);
				const float CollapsedFogParameter = FogInfo.FogDensity * FMath::Pow(2.0f, CollapsedFogParameterPower);
				View.ExponentialFogParameters = FVector4(CollapsedFogParameter, FogInfo.FogHeightFalloff, CosTerminatorAngle, FogInfo.StartDistance);
				View.ExponentialFogColor = FVector(FogInfo.FogColor.R, FogInfo.FogColor.G, FogInfo.FogColor.B);
				View.FogMaxOpacity = FogInfo.FogMaxOpacity;
				View.ExponentialFogParameters3 = FVector2D(FogInfo.FogDensity, FogInfo.FogHeight);

				View.DirectionalInscatteringExponent = FogInfo.DirectionalInscatteringExponent;
				View.DirectionalInscatteringStartDistance = FogInfo.DirectionalInscatteringStartDistance;
				View.bUseDirectionalInscattering = false;
				View.InscatteringLightDirection = FVector(0);

				for (TSparseArray<FLightSceneInfoCompact>::TConstIterator It(Scene->Lights); It; ++It)
				{
					const FLightSceneInfoCompact& LightInfo = *It;

					// This will find the first directional light that is set to be used as an atmospheric sun light of sufficient brightness.
					// If you have more than one directional light with these properties then all subsequent lights will be ignored.
					if (LightInfo.LightSceneInfo->Proxy->GetLightType() == LightType_Directional
						&& LightInfo.LightSceneInfo->Proxy->IsUsedAsAtmosphereSunLight()
						&& LightInfo.LightSceneInfo->Proxy->GetColor().ComputeLuminance() > KINDA_SMALL_NUMBER
						&& FogInfo.DirectionalInscatteringColor.ComputeLuminance() > KINDA_SMALL_NUMBER)
					{
						View.InscatteringLightDirection = -LightInfo.LightSceneInfo->Proxy->GetDirection();
						View.bUseDirectionalInscattering = true;
						View.DirectionalInscatteringColor = FogInfo.DirectionalInscatteringColor * LightInfo.LightSceneInfo->Proxy->GetColor().ComputeLuminance();
						break;
					}
				}
			}
		}
	}
}

FGlobalBoundShaderState ExponentialBoundShaderState;

/** Sets the bound shader state for either the per-pixel or per-sample fog pass. */
void SetFogShaders(FRHICommandList& RHICmdList, FScene* Scene, const FViewInfo& View, const FLightShaftsOutput& LightShaftsOutput)
{
	if (Scene->ExponentialFogs.Num() > 0)
	{
		TShaderMapRef<FHeightFogVS> VertexShader(View.ShaderMap);
		TShaderMapRef<FExponentialHeightFogPS> ExponentialHeightFogPixelShader(View.ShaderMap);

		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), ExponentialBoundShaderState, GFogVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ExponentialHeightFogPixelShader);
		VertexShader->SetParameters(RHICmdList, View);
		ExponentialHeightFogPixelShader->SetParameters(RHICmdList, View, LightShaftsOutput);
	}
}

bool FDeferredShadingSceneRenderer::RenderFog(FRHICommandListImmediate& RHICmdList, const FLightShaftsOutput& LightShaftsOutput)
{
	if (Scene->ExponentialFogs.Num() > 0 && !Scene->ReadOnlyCVARCache.bEnableVertexFoggingForOpaque)
	{
		static const FVector2D Vertices[4] =
		{
			FVector2D(-1,-1),
			FVector2D(-1,+1),
			FVector2D(+1,+1),
			FVector2D(+1,-1),
		};
		static const uint16 Indices[6] =
		{
			0, 1, 2,
			0, 2, 3
		};
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			SCOPED_DRAW_EVENTF(RHICmdList, Fog, TEXT("ExponentialHeightFog %dx%d"), View.ViewRect.Width(), View.ViewRect.Height());
			SCOPED_GPU_STAT(RHICmdList, Stat_GPU_Fog);

			if (View.IsPerspectiveProjection() == false)
			{
				continue; // Do not render exponential fog in orthographic views.
			}

			// Set the device viewport for the view.
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			
			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
			
			// disable alpha writes in order to preserve scene depth values on PC
			RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha>::GetRHI());

			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

			SetFogShaders(RHICmdList, Scene,View,LightShaftsOutput);

			// Draw a quad covering the view.
			DrawIndexedPrimitiveUP(
				RHICmdList,
				PT_TriangleList,
				0,
				ARRAY_COUNT(Vertices),
				2,
				Indices,
				sizeof(Indices[0]),
				Vertices,
				sizeof(Vertices[0])
				);
		}

		return true;
	}

	return false;
}


bool ShouldRenderFog(const FSceneViewFamily& Family)
{
	const FEngineShowFlags EngineShowFlags = Family.EngineShowFlags;

	return EngineShowFlags.Fog
		&& EngineShowFlags.Materials 
		&& !Family.UseDebugViewPS()
		&& CVarFog.GetValueOnRenderThread() == 1
		&& !EngineShowFlags.StationaryLightOverlap 
		&& !EngineShowFlags.LightMapDensity;
}
