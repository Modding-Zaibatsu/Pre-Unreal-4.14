// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMaterial.cpp: Post processing Material implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessMaterial.h"
#include "PostProcessing.h"
#include "PostProcessEyeAdaptation.h"
#include "../../../Engine/Public/TileRendering.h"
#include "SceneUtils.h"

enum class EPostProcessMaterialTarget
{
	HighEnd,
	Mobile
};

static bool ShouldCachePostProcessMaterial(EPostProcessMaterialTarget MaterialTarget, EShaderPlatform Platform, const FMaterial* Material)
{
	if (Material->GetMaterialDomain() == MD_PostProcess)
	{
		switch (MaterialTarget)
		{
		case EPostProcessMaterialTarget::HighEnd:
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
		case EPostProcessMaterialTarget::Mobile:
			return IsMobilePlatform(Platform) && IsMobileHDR();
		}
	}

	return false;
}

template<EPostProcessMaterialTarget MaterialTarget>
class FPostProcessMaterialVS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FPostProcessMaterialVS, Material);
public:

	/**
	  * Only compile these shaders for post processing domain materials
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return ShouldCachePostProcessMaterial(MaterialTarget, Platform, Material);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL"), 1);

		if (MaterialTarget == EPostProcessMaterialTarget::Mobile)
		{
			OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL_BEFORE_TONEMAP"), (Material->GetBlendableLocation() != BL_AfterTonemapping) ? 1 : 0);
		}
	}
	
	FPostProcessMaterialVS( )	{ }
	FPostProcessMaterialVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context )
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FMaterialShader::SetViewParameters(RHICmdList, ShaderRHI, Context.View, Context.View.ViewUniformBuffer);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}

	// Begin FShader interface
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
	//  End FShader interface 
private:
	FPostProcessPassParameters PostprocessParameter;
};

typedef FPostProcessMaterialVS<EPostProcessMaterialTarget::HighEnd> FPostProcessMaterialVS_HighEnd;
typedef FPostProcessMaterialVS<EPostProcessMaterialTarget::Mobile> FPostProcessMaterialVS_Mobile;

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,FPostProcessMaterialVS_HighEnd,TEXT("PostProcessMaterialShaders"),TEXT("MainVS"),SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,FPostProcessMaterialVS_Mobile,TEXT("PostProcessMaterialShaders"),TEXT("MainVS_ES2"),SF_Vertex);

/**
 * A pixel shader for rendering a post process material
 */
template<EPostProcessMaterialTarget MaterialTarget>
class FPostProcessMaterialPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FPostProcessMaterialPS,Material);
public:

	/**
	  * Only compile these shaders for post processing domain materials
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return ShouldCachePostProcessMaterial(MaterialTarget, Platform, Material);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL"), 1);
		
		if (MaterialTarget == EPostProcessMaterialTarget::Mobile)
		{
			OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL_BEFORE_TONEMAP"), (Material->GetBlendableLocation() != BL_AfterTonemapping) ? 1 : 0);
		}
	}

	FPostProcessMaterialPS() {}
	FPostProcessMaterialPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMaterialShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context, const FMaterialRenderProxy* Material )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, Material, *Material->GetMaterial(Context.View.GetFeatureLevel()), Context.View, Context.View.ViewUniformBuffer, true, ESceneRenderTargetsMode::SetTextures);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FPostProcessPassParameters PostprocessParameter;
};

typedef FPostProcessMaterialPS<EPostProcessMaterialTarget::HighEnd> FFPostProcessMaterialPS_HighEnd;
typedef FPostProcessMaterialPS<EPostProcessMaterialTarget::Mobile> FPostProcessMaterialPS_Mobile;

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,FFPostProcessMaterialPS_HighEnd,TEXT("PostProcessMaterialShaders"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,FPostProcessMaterialPS_Mobile,TEXT("PostProcessMaterialShaders"),TEXT("MainPS_ES2"),SF_Pixel);

FRCPassPostProcessMaterial::FRCPassPostProcessMaterial(UMaterialInterface* InMaterialInterface, ERHIFeatureLevel::Type InFeatureLevel, EPixelFormat OutputFormatIN)
: MaterialInterface(InMaterialInterface), OutputFormat(OutputFormatIN)
{
	FMaterialRenderProxy* Proxy = MaterialInterface->GetRenderProxy(false);
	check(Proxy);

	const FMaterial* Material = Proxy->GetMaterialNoFallback(InFeatureLevel);
	
	if (!Material || Material->GetMaterialDomain() != MD_PostProcess)
	{
		MaterialInterface = UMaterial::GetDefaultMaterial(MD_PostProcess);
	}
}
		
/** The filter vertex declaration resource type. */
class FPostProcessMaterialVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	
	/** Destructor. */
	virtual ~FPostProcessMaterialVertexDeclaration() {}
	
	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FFilterVertex);
		Elements.Add(FVertexElement(0,STRUCT_OFFSET(FFilterVertex,Position),VET_Float4,0,Stride));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}
	
	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
TGlobalResource<FPostProcessMaterialVertexDeclaration> GPostProcessMaterialVertexDeclaration;

void FRCPassPostProcessMaterial::Process(FRenderingCompositePassContext& Context)
{
	FMaterialRenderProxy* Proxy = MaterialInterface->GetRenderProxy(false);

	check(Proxy);

	ERHIFeatureLevel::Type FeatureLevel = Context.View.GetFeatureLevel();
	
	const FMaterial* Material = Proxy->GetMaterial(FeatureLevel);
	
	check(Material);

	const FSceneView& View = Context.View;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessMaterial, TEXT("PostProcessMaterial %dx%d Material=%s"), View.ViewRect.Width(), View.ViewRect.Height(), *Material->GetFriendlyName());

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneViewFamily& ViewFamily = *(View.Family);

	// hacky cast
	FRenderingCompositePassContext RenderingCompositePassContext(Context.RHICmdList, (FViewInfo&)View);
	RenderingCompositePassContext.Pass = this;

	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIParamRef());

	if( ViewFamily.RenderTarget->GetRenderTargetTexture() != DestRenderTarget.TargetableTexture )
	{
		Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, View.ViewRect);
	}

	Context.SetViewportAndCallRHI(View.ViewRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();
	FShader* VertexShader = nullptr;
	if (FeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		FPostProcessMaterialPS_Mobile* PixelShader_Mobile = MaterialShaderMap->GetShader<FPostProcessMaterialPS_Mobile>();
		FPostProcessMaterialVS_Mobile* VertexShader_Mobile = MaterialShaderMap->GetShader<FPostProcessMaterialVS_Mobile>();
		Context.RHICmdList.SetLocalBoundShaderState(Context.RHICmdList.BuildLocalBoundShaderState(GFilterVertexDeclaration.VertexDeclarationRHI, VertexShader_Mobile->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), PixelShader_Mobile->GetPixelShader(), FGeometryShaderRHIRef()));
		VertexShader_Mobile->SetParameters(Context.RHICmdList, Context);
		PixelShader_Mobile->SetParameters(Context.RHICmdList, Context, MaterialInterface->GetRenderProxy(false));
		VertexShader = VertexShader_Mobile;
	}
	else
	{
		FFPostProcessMaterialPS_HighEnd* PixelShader_HighEnd = MaterialShaderMap->GetShader<FFPostProcessMaterialPS_HighEnd>();
		FPostProcessMaterialVS_HighEnd* VertexShader_HighEnd = MaterialShaderMap->GetShader<FPostProcessMaterialVS_HighEnd>();
		Context.RHICmdList.SetLocalBoundShaderState(Context.RHICmdList.BuildLocalBoundShaderState(GPostProcessMaterialVertexDeclaration.VertexDeclarationRHI, VertexShader_HighEnd->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), PixelShader_HighEnd->GetPixelShader(), FGeometryShaderRHIRef()));
		VertexShader_HighEnd->SetParameters(Context.RHICmdList, Context);
		PixelShader_HighEnd->SetParameters(Context.RHICmdList, Context, MaterialInterface->GetRenderProxy(false));
		VertexShader = VertexShader_HighEnd;
	}

	DrawPostProcessPass(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		VertexShader,
		View.StereoPass,
		Context.HasHmdMesh(),
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	if(Material->NeedsGBuffer())
	{
		FSceneRenderTargets::Get(Context.RHICmdList).AdjustGBufferRefCount(Context.RHICmdList,-1);
	}
}

FPooledRenderTargetDesc FRCPassPostProcessMaterial::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	if (OutputFormat != PF_Unknown)
	{
		Ret.Format = OutputFormat;
	}
	Ret.Reset();
	Ret.AutoWritable = false;
	Ret.DebugName = TEXT("PostProcessMaterial");

	return Ret;
}
