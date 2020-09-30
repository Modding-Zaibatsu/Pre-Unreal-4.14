// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "RendererPrivate.h"

#if WITH_EDITOR

#include "PostProcessCompositeEditorPrimitives.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "SceneUtils.h"


// temporary
static TAutoConsoleVariable<float> CVarEditorOpaqueGizmo(
	TEXT("r.Editor.OpaqueGizmo"),
	0.0f,
	TEXT("0..1\n0: occluded gizmo is partly transparent (default), 1:gizmo is never occluded"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarEditorMovingPattern(
	TEXT("r.Editor.MovingPattern"),
	1.0f,
	TEXT("0:animation over time is off (default is 1)"),
	ECVF_RenderThreadSafe);

/**
 * Pixel shader for compositing editor primitives rendered                   
 */
template<uint32 MSAASampleCount>
class FPostProcessCompositeEditorPrimitivesPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE( FPostProcessCompositeEditorPrimitivesPS, Global )

	static bool ShouldCache(EShaderPlatform Platform)
	{
		if(!IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5))
		{
			if(MSAASampleCount > 1)
			{
				return false;
			}
		}

		return IsPCPlatform(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("MSAA_SAMPLE_COUNT"), MSAASampleCount);
	}

	FPostProcessCompositeEditorPrimitivesPS() {}

public:


	/** FPostProcessPassParameters constructor. */
	FPostProcessCompositeEditorPrimitivesPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostProcessParameters.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		EditorPrimitivesDepth.Bind(Initializer.ParameterMap,TEXT("EditorPrimitivesDepth"));
		EditorPrimitivesColor.Bind(Initializer.ParameterMap,TEXT("EditorPrimitivesColor"));
		EditorPrimitivesColorSampler.Bind(Initializer.ParameterMap,TEXT("EditorPrimitivesColorSampler"));
		EditorRenderParams.Bind(Initializer.ParameterMap,TEXT("EditorRenderParams"));
		FilteredSceneDepthTexture.Bind(Initializer.ParameterMap,TEXT("FilteredSceneDepthTexture"));
		FilteredSceneDepthTextureSampler.Bind(Initializer.ParameterMap,TEXT("FilteredSceneDepthTextureSampler"));
	}

	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		FSamplerStateRHIRef SamplerStateRHIRef = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
		PostProcessParameters.SetPS(ShaderRHI, Context, SamplerStateRHIRef);
		if(MSAASampleCount > 1)
		{
			SetTextureParameter(Context.RHICmdList, ShaderRHI, EditorPrimitivesColor, SceneContext.EditorPrimitivesColor->GetRenderTargetItem().TargetableTexture);
			SetTextureParameter(Context.RHICmdList, ShaderRHI, EditorPrimitivesDepth, SceneContext.EditorPrimitivesDepth->GetRenderTargetItem().TargetableTexture);
		}
		else
		{
			SetTextureParameter(Context.RHICmdList, ShaderRHI, EditorPrimitivesColor, EditorPrimitivesColorSampler, SamplerStateRHIRef, SceneContext.EditorPrimitivesColor->GetRenderTargetItem().ShaderResourceTexture);
			SetTextureParameter(Context.RHICmdList, ShaderRHI, EditorPrimitivesDepth, SceneContext.EditorPrimitivesDepth->GetRenderTargetItem().ShaderResourceTexture);
		}

		{
			FLinearColor Value(CVarEditorOpaqueGizmo.GetValueOnRenderThread(), CVarEditorMovingPattern.GetValueOnRenderThread(), 0, 0);

			const FSceneViewFamily& ViewFamily = *(Context.View.Family);

			if(ViewFamily.EngineShowFlags.Wireframe)
			{
				// no occlusion in wire frame rendering
				Value.R = 1;
			}

			if(!ViewFamily.bRealtimeUpdate)
			{
				// no animation if realtime update is disabled
				Value.G = 0;
			}

			SetShaderValue(Context.RHICmdList, ShaderRHI, EditorRenderParams, Value);
		}

		if(FilteredSceneDepthTexture.IsBound())
		{
			const FTexture2DRHIRef* DepthTexture = SceneContext.GetActualDepthTexture();
			SetTextureParameter(
				Context.RHICmdList,
				ShaderRHI,
				FilteredSceneDepthTexture,
				FilteredSceneDepthTextureSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				*DepthTexture
				);
		}
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostProcessParameters << EditorPrimitivesColor << EditorPrimitivesColorSampler << EditorPrimitivesDepth << DeferredParameters << EditorRenderParams;
		Ar << FilteredSceneDepthTexture;
		Ar << FilteredSceneDepthTextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter EditorPrimitivesColor;
	FShaderResourceParameter EditorPrimitivesColorSampler;
	FShaderResourceParameter EditorPrimitivesDepth;
	FPostProcessPassParameters PostProcessParameters;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter EditorRenderParams;
	/** Parameter for reading filtered depth values */
	FShaderResourceParameter FilteredSceneDepthTexture; 
	FShaderResourceParameter FilteredSceneDepthTextureSampler; 

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessCompositeEditorPrimitives");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) typedef FPostProcessCompositeEditorPrimitivesPS<A> FPostProcessCompositeEditorPrimitivesPS##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessCompositeEditorPrimitivesPS##A, SF_Pixel);

	VARIATION1(1)			VARIATION1(2)			VARIATION1(4)			VARIATION1(8)
#undef VARIATION1


template <uint32 MSAASampleCount>
static void SetCompositePrimitivesShaderTempl(const FRenderingCompositePassContext& Context)
{
	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);
	TShaderMapRef<FPostProcessCompositeEditorPrimitivesPS<MSAASampleCount> > PixelShader(ShaderMap);

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, FeatureLevel, BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context);
}

void FRCPassPostProcessCompositeEditorPrimitives::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, CompositeEditorPrimitives);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FViewInfo& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	// If we render wirframe we already started rendering to the EditorPrimitives buffer, so we don't want to clear it.
	bool bClearIsNeeded = !IsValidRef(SceneContext.EditorPrimitivesColor);

	// Get or create the msaa depth and color buffer
	FTexture2DRHIRef ColorTarget = SceneContext.GetEditorPrimitivesColor(Context.RHICmdList);
	FTexture2DRHIRef DepthTarget = SceneContext.GetEditorPrimitivesDepth(Context.RHICmdList);

	FTextureRHIParamRef EditorRenderTargets[2];
	EditorRenderTargets[0] = ColorTarget;
	EditorRenderTargets[1] = DepthTarget;

	const uint32 MSAASampleCount = SceneContext.EditorPrimitivesColor->GetDesc().NumSamples;

	{
		SetRenderTarget(Context.RHICmdList, ColorTarget, DepthTarget, ESimpleRenderTargetMode::EExistingColorAndDepth);
		Context.SetViewportAndCallRHI(DestRect);

		if (bClearIsNeeded)
		{
			SCOPED_DRAW_EVENT(Context.RHICmdList, ClearViewEditorPrimitives);
			// Clear color and depth
			Context.RHICmdList.Clear(true, FLinearColor(0, 0, 0, 0), true, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect());
		}

		SCOPED_DRAW_EVENT(Context.RHICmdList, RenderEditorPrimitives);

		Context.RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());

		if (bDeferredBasePass)
		{
			RenderPrimitivesToComposite<FBasePassOpaqueDrawingPolicyFactory>(Context.RHICmdList, View);
		}
		else
		{
			RenderPrimitivesToComposite<FMobileBasePassOpaqueDrawingPolicyFactory>(Context.RHICmdList, View);
		}

		GRenderTargetPool.VisualizeTexture.SetCheckPoint(Context.RHICmdList, SceneContext.EditorPrimitivesColor);
		Context.RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EditorRenderTargets, 2);
	}

	//	Context.RHICmdList.CopyToResolveTarget(SceneContext.EditorPrimitivesColor->GetRenderTargetItem().TargetableTexture, SceneContext.EditorPrimitivesColor->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	const FTexture2DRHIRef& DestRenderTargetSurface = (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture;

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTargetSurface, FTextureRHIRef());

	Context.SetViewportAndCallRHI(DestRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	if(MSAASampleCount == 1)
	{
		SetCompositePrimitivesShaderTempl<1>(Context);
	}
	else if(MSAASampleCount == 2)
	{
		SetCompositePrimitivesShaderTempl<2>(Context);
	}
	else if(MSAASampleCount == 4)
	{
		SetCompositePrimitivesShaderTempl<4>(Context);
	}
	else if(MSAASampleCount == 8)
	{
		SetCompositePrimitivesShaderTempl<8>(Context);
	}
	else
	{
		// not supported, internal error
		check(0);
	}

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	// Draw a quad mapping our render targets to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTargetSurface, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// Clean up targets
	SceneContext.CleanUpEditorPrimitiveTargets();
}

FPooledRenderTargetDesc FRCPassPostProcessCompositeEditorPrimitives::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("EditorPrimitives");

	return Ret;
}

template<typename TBasePass>
void FRCPassPostProcessCompositeEditorPrimitives::RenderPrimitivesToComposite(FRHICommandListImmediate& RHICmdList, const FViewInfo& View)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	// Always depth test against other editor primitives
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());

	// most objects should be occluded by the existing scene so we do a manual depth test in the shader
	bool bDepthTest = true;

	const auto FeatureLevel = View.GetFeatureLevel();
	const auto ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];
	const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(ShaderPlatform);
	FTexture2DRHIRef SceneDepth = SceneContext.GetSceneDepthTexture();

	typename TBasePass::ContextType Context(true, ESceneRenderTargetsMode::SetTextures);

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicEditorMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicEditorMeshElements[MeshBatchIndex];

		if (MeshBatchAndRelevance.GetHasOpaqueOrMaskedMaterial() || View.Family->EngineShowFlags.Wireframe)
		{
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			TBasePass::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
		}
	}

	View.EditorSimpleElementCollector.DrawBatchedElements(RHICmdList, View, SceneContext.GetSceneDepthTexture(), EBlendModeFilter::OpaqueAndMasked);
	
	// Draw the base pass for the view's batched mesh elements.
	DrawViewElements<TBasePass>(RHICmdList, View, typename TBasePass::ContextType(bDepthTest, ESceneRenderTargetsMode::SetTextures), SDPG_World, false);

	// Draw the view's batched simple elements(lines, sprites, etc).
	View.BatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View, false, 1.0f, SceneDepth);

	// Draw foreground objects. Draw twice, once without depth testing to bring them into the foreground and again to depth test against themselves
	{
		// Do not test against non-composited objects
		bDepthTest = false;

		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		DrawViewElements<TBasePass>(RHICmdList, View, typename TBasePass::ContextType(bDepthTest, ESceneRenderTargetsMode::SetTextures), SDPG_Foreground, false);

		View.TopBatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View, false);

		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());

		DrawViewElements<TBasePass>(RHICmdList, View, typename TBasePass::ContextType(bDepthTest, ESceneRenderTargetsMode::SetTextures), SDPG_Foreground, false);

		View.TopBatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View, false);
	}
}

#endif