// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeRendering.cpp: Contains definitions for rendering debug viewmodes.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "SceneUtils.h"
#include "PostProcessing.h"
#include "PrimitiveDistanceAccuracyRendering.h"
#include "MeshTexCoordSizeAccuracyRendering.h"
#include "MaterialTexCoordScalesRendering.h"

IMPLEMENT_MATERIAL_SHADER_TYPE(,FDebugViewModeVS,TEXT("DebugViewModeVertexShader"),TEXT("Main"),SF_Vertex);	
IMPLEMENT_MATERIAL_SHADER_TYPE(,FDebugViewModeHS,TEXT("DebugViewModeVertexShader"),TEXT("MainHull"),SF_Hull);	
IMPLEMENT_MATERIAL_SHADER_TYPE(,FDebugViewModeDS,TEXT("DebugviewModeVertexShader"),TEXT("MainDomain"),SF_Domain);

/**
* Pixel shader that renders texture streamer wanted mips accuracy.
*/
class FMissingShaderPS : public FGlobalShader, public IDebugViewModePSInterface
{
	DECLARE_SHADER_TYPE(FMissingShaderPS,Global);

public:

	static bool ShouldCache(EShaderPlatform Platform) { return AllowDebugViewVSDSHS(Platform); }

	FMissingShaderPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):	FGlobalShader(Initializer) {}
	FMissingShaderPS() {}

	virtual bool Serialize(FArchive& Ar) override { return FGlobalShader::Serialize(Ar); }

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment) {}

	virtual void SetParameters(
		FRHICommandList& RHICmdList, 
		const FShader* OriginalVS, 
		const FShader* OriginalPS, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView& View
		) override {}

	virtual void SetMesh(
		FRHICommandList& RHICmdList, 
		const FVertexFactory* VertexFactory,
		const FSceneView& View,
		const FPrimitiveSceneProxy* Proxy,
		int32 VisualizeLODIndex,
		const FMeshBatchElement& BatchElement, 
		const FMeshDrawingRenderState& DrawRenderState
		) override{}

	virtual void SetMesh(FRHICommandList& RHICmdList, const FSceneView& View) override {}

	virtual FShader* GetShader() override { return static_cast<FShader*>(this); }
};

IMPLEMENT_SHADER_TYPE(,FMissingShaderPS,TEXT("MissingShaderPixelShader"),TEXT("Main"),SF_Pixel);


void FDebugViewMode::GetMaterialForVSHSDS(const FMaterialRenderProxy** MaterialRenderProxy, const FMaterial** Material, ERHIFeatureLevel::Type FeatureLevel)
{
	// If the material was compiled fo VS, return it, otherwise, return the default material.
	if (!(*Material)->HasVertexPositionOffsetConnected() && (*Material)->GetTessellationMode() == MTM_NoTessellation)
	{
		if (MaterialRenderProxy)
		{
			*MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
		}
		*Material = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false)->GetMaterial(FeatureLevel);
	}
}

ENGINE_API const FMaterial* GetDebugViewMaterialPS(EDebugViewShaderMode DebugViewShaderMode, const FMaterial* Material);

IDebugViewModePSInterface* FDebugViewMode::GetPSInterface(TShaderMap<FGlobalShaderType>* ShaderMap, const FMaterial* Material, EDebugViewShaderMode DebugViewShaderMode)
{
    // The mesh material shaders are only compiled with the local vertex factory to prevent multiple compilation.
    // Nothing from the factory is actually used, but the shader must still derive from FMeshMaterialShader.
    // This is required to call FMeshMaterialShader::SetMesh and bind primitive related data.
	static FVertexFactoryType* LocalVertexFactory = FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find));

	switch (DebugViewShaderMode)
	{
	case DVSM_QuadComplexity:
	case DVSM_ShaderComplexityBleedingQuadOverhead:
		return *TShaderMapRef<TQuadComplexityAccumulatePS>(ShaderMap);
	case DVSM_ShaderComplexity:
	case DVSM_ShaderComplexityContainedQuadOverhead:
		return *TShaderMapRef<TShaderComplexityAccumulatePS>(ShaderMap);
	case DVSM_PrimitiveDistanceAccuracy:
		return *TShaderMapRef<FPrimitiveDistanceAccuracyPS>(ShaderMap);
	case DVSM_MeshTexCoordSizeAccuracy:
		return *TShaderMapRef<FMeshTexCoordSizeAccuracyPS>(ShaderMap);
	case DVSM_MaterialTexCoordScalesAccuracy:
	case DVSM_MaterialTexCoordScalesAnalysis:
	{
		const FMaterial* MaterialForPS = GetDebugViewMaterialPS(DebugViewShaderMode, Material);
		if (MaterialForPS)
		{
			return MaterialForPS->GetShader<FMaterialTexCoordScalePS>(LocalVertexFactory);
		}
		break;
	}
	default:
		break;
	}

	return *TShaderMapRef<FMissingShaderPS>(ShaderMap);
}

void FDebugViewMode::PatchBoundShaderState(
	FBoundShaderStateInput& BoundShaderStateInput,
	const FMaterial* Material,
	const FVertexFactory* VertexFactory,
	ERHIFeatureLevel::Type FeatureLevel, 
	EDebugViewShaderMode DebugViewShaderMode
	)
{
	const FMaterial* MaterialForPS = Material; // Backup before calling GetMaterialForVSHSDS

	if (AllowDebugViewVSDSHS(GetFeatureLevelShaderPlatform(FeatureLevel)))
	{
		GetMaterialForVSHSDS(nullptr, &Material, FeatureLevel);

		FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

		BoundShaderStateInput.VertexShaderRHI = Material->GetShader<FDebugViewModeVS>(VertexFactoryType)->GetVertexShader();

		if (BoundShaderStateInput.HullShaderRHI)
		{
			BoundShaderStateInput.HullShaderRHI = Material->GetShader<FDebugViewModeHS>(VertexFactoryType)->GetHullShader();
		}
		if (BoundShaderStateInput.DomainShaderRHI)
		{
			BoundShaderStateInput.DomainShaderRHI = Material->GetShader<FDebugViewModeDS>(VertexFactoryType)->GetDomainShader();
		}
	}

	BoundShaderStateInput.PixelShaderRHI = GetPSInterface(GetGlobalShaderMap(FeatureLevel), MaterialForPS, DebugViewShaderMode)->GetShader()->GetPixelShader();
}

void FDebugViewMode::SetParametersVSHSDS(
	FRHICommandList& RHICmdList, 
	const FMaterialRenderProxy* MaterialRenderProxy, 
	const FMaterial* Material, 
	const FSceneView& View,
	const FVertexFactory* VertexFactory,
	bool bHasHullAndDomainShader
	)
{
	VertexFactory->Set(RHICmdList);

	GetMaterialForVSHSDS(&MaterialRenderProxy, &Material, View.GetFeatureLevel());

	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

	Material->GetShader<FDebugViewModeVS>(VertexFactoryType)->SetParameters(RHICmdList, MaterialRenderProxy, *Material, View);

	if (bHasHullAndDomainShader)
	{
		Material->GetShader<FDebugViewModeHS>(VertexFactoryType)->SetParameters(RHICmdList, MaterialRenderProxy, View);
		Material->GetShader<FDebugViewModeDS>(VertexFactoryType)->SetParameters(RHICmdList, MaterialRenderProxy, View);
	}
}

void FDebugViewMode::SetMeshVSHSDS(
	FRHICommandList& RHICmdList, 
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	const FMeshBatchElement& BatchElement, 
	const FMeshDrawingRenderState& DrawRenderState,
	const FMaterial* Material, 
	bool bHasHullAndDomainShader
	)
{
	GetMaterialForVSHSDS(nullptr, &Material, View.GetFeatureLevel());

	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

	Material->GetShader<FDebugViewModeVS>(VertexFactoryType)->SetMesh(RHICmdList, VertexFactory, View, Proxy, BatchElement, DrawRenderState);

	if (bHasHullAndDomainShader)
	{
		Material->GetShader<FDebugViewModeHS>(VertexFactoryType)->SetMesh(RHICmdList, VertexFactory, View, Proxy, BatchElement, DrawRenderState);
		Material->GetShader<FDebugViewModeDS>(VertexFactoryType)->SetMesh(RHICmdList, VertexFactory, View, Proxy, BatchElement, DrawRenderState);
	}
}