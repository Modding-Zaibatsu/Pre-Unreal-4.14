//-----------------------------------------------------------------------------
// File:		LightPropagationVolumeVisualisation.cpp
//
// Summary:		Light Propagation Volume visualisation support 
//
// Created:		2013-03-01
//
// Author:		mailto:benwood@microsoft.com
//
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "LightPropagationVolume.h"
#include "SceneUtils.h"

// ----------------------------------------------------------------------------

static FGlobalBoundShaderState LpvVisBoundShaderState;

// ----------------------------------------------------------------------------

class FLpvVisualiseBase : public FGlobalShader
{
public:
	// Default constructor
	FLpvVisualiseBase()	{	}

	// Initialization constructor 
	explicit FLpvVisualiseBase( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )				
	{ 
		OutEnvironment.SetDefine( TEXT("LPV_MULTIPLE_BOUNCES"), (uint32)LPV_MULTIPLE_BOUNCES );
		OutEnvironment.SetDefine( TEXT("LPV_GV_SH_ORDER"),		(uint32)LPV_GV_SH_ORDER );

		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment ); 
	}
};


class FLpvVisualiseGS : public FLpvVisualiseBase
{
public:
	DECLARE_SHADER_TYPE(FLpvVisualiseGS,Global);

	FLpvVisualiseGS()																												{}
	explicit FLpvVisualiseGS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvVisualiseBase(Initializer)	{}
	virtual bool Serialize( FArchive& Ar ) override																					{ return FLpvVisualiseBase::Serialize( Ar ); }

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && RHISupportsGeometryShaders(Platform) && IsLPVSupported(Platform);
	}
	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )				{ FLpvVisualiseBase::ModifyCompilationEnvironment( Platform, OutEnvironment ); }

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View )
	{
		FGeometryShaderRHIParamRef ShaderRHI = GetGeometryShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View); 
	}
};

class FLpvVisualiseVS : public FLpvVisualiseBase
{
public:
	DECLARE_SHADER_TYPE(FLpvVisualiseVS,Global);

	FLpvVisualiseVS()	{	}
	explicit FLpvVisualiseVS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvVisualiseBase(Initializer) {}
	virtual bool Serialize( FArchive& Ar ) override																					{ return FLpvVisualiseBase::Serialize( Ar ); }

	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }
	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )				{ FLpvVisualiseBase::ModifyCompilationEnvironment( Platform, OutEnvironment ); }

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View )
	{
		FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View); 
	}
};

class FLpvVisualisePS : public FLpvVisualiseBase
{
public:
	DECLARE_SHADER_TYPE(FLpvVisualisePS,Global);

	FLpvVisualisePS()	{	}
	explicit FLpvVisualisePS( const ShaderMetaType::CompiledShaderInitializerType& Initializer ) : FLpvVisualiseBase(Initializer) 
	{
		for ( int i = 0; i < 7; i++ )
		{
			LpvBufferSRVParameters[i].Bind( Initializer.ParameterMap, LpvVolumeTextureSRVNames[i] );
		}

		LpvVolumeTextureSampler.Bind(Initializer.ParameterMap, TEXT("gLpv3DTextureSampler"));
		for ( int i = 0; i < NUM_GV_TEXTURES; i++ ) 
		{
			GvBufferSRVParameters[i].Bind( Initializer.ParameterMap, LpvGvVolumeTextureSRVNames[i] );
		}
	}

	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && IsLPVSupported(Platform); }
	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )				{ FLpvVisualiseBase::ModifyCompilationEnvironment( Platform, OutEnvironment ); }

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FLightPropagationVolume* LPV,
		const FSceneView& View )
	{
		FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View); 
		
		for ( int i = 0; i < 7; i++ )
		{
			FTextureRHIParamRef LpvBufferSrv = LPV->LpvVolumeTextures[ 1-LPV->mWriteBufferIndex ][i]->GetRenderTargetItem().ShaderResourceTexture;
			if ( LpvBufferSRVParameters[i].IsBound() )
			{
				RHICmdList.SetShaderTexture(ShaderRHI, LpvBufferSRVParameters[i].GetBaseIndex(), LpvBufferSrv);
			}
			SetTextureParameter(RHICmdList, ShaderRHI, LpvBufferSRVParameters[i], LpvVolumeTextureSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), LpvBufferSrv );
		}
		
		for ( int i = 0; i < NUM_GV_TEXTURES; i++ ) 
		{
			FTextureRHIParamRef GvBufferSrv = LPV->GvVolumeTextures[i]->GetRenderTargetItem().ShaderResourceTexture;
			if ( GvBufferSRVParameters[i].IsBound() )
			{
				RHICmdList.SetShaderTexture(ShaderRHI, GvBufferSRVParameters[i].GetBaseIndex(), GvBufferSrv);
			}
			SetTextureParameter(RHICmdList, ShaderRHI, GvBufferSRVParameters[i], LpvVolumeTextureSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), GvBufferSrv );
		}

	}


	// Serialization
	virtual bool Serialize( FArchive& Ar ) override
	{
		bool bShaderHasOutdatedParameters = FLpvVisualiseBase::Serialize( Ar );
		
		for ( int i = 0; i < 7; i++ )
		{
			Ar << LpvBufferSRVParameters[i];
		}

		Ar << LpvVolumeTextureSampler;

		for ( int i = 0; i < NUM_GV_TEXTURES; i++ ) 
		{
			Ar << GvBufferSRVParameters[i];
		}
		return bShaderHasOutdatedParameters;
	}
	FShaderResourceParameter LpvBufferSRVParameters[7];

	FShaderResourceParameter LpvVolumeTextureSampler;

	FShaderResourceParameter GvBufferSRVParameters[3];

	void UnbindBuffers(FRHICommandList& RHICmdList)
	{
		// TODO: Is this necessary here?
		FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		for ( int i = 0; i < 7; i++ )
		{
			if ( LpvBufferSRVParameters[i].IsBound() )
			{
				RHICmdList.SetShaderTexture(ShaderRHI, LpvBufferSRVParameters[i].GetBaseIndex(), FTextureRHIParamRef());
			}
		}
		
		for ( int i = 0; i < NUM_GV_TEXTURES; i++ ) 
		{
			if ( GvBufferSRVParameters[i].IsBound() )
			{
				RHICmdList.SetShaderTexture(ShaderRHI, GvBufferSRVParameters[i].GetBaseIndex(), FTextureRHIParamRef());
			}
		}
	}

};


IMPLEMENT_SHADER_TYPE(,FLpvVisualiseGS,TEXT("LPVVisualise"),TEXT("GShader"),SF_Geometry);
IMPLEMENT_SHADER_TYPE(,FLpvVisualiseVS,TEXT("LPVVisualise"),TEXT("VShader"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FLpvVisualisePS,TEXT("LPVVisualise"),TEXT("PShader"),SF_Pixel);


void FLightPropagationVolume::Visualise(FRHICommandList& RHICmdList, const FViewInfo& View) const
{
	SCOPED_DRAW_EVENT(RHICmdList, LpvVisualise);
	check(View.GetFeatureLevel() == ERHIFeatureLevel::SM5);

	TShaderMapRef<FLpvVisualiseVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FLpvVisualiseGS> GeometryShader(View.ShaderMap);
	TShaderMapRef<FLpvVisualisePS> PixelShader(View.ShaderMap);

	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());

	SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), LpvVisBoundShaderState, GSimpleElementVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, *GeometryShader);

	VertexShader->SetParameters(RHICmdList, View);
	GeometryShader->SetParameters(RHICmdList, View);
	PixelShader->SetParameters(RHICmdList, this, View);

	RHICmdList.SetStreamSource(0, NULL, 0, 0);
	RHICmdList.DrawPrimitive(PT_PointList, 0, 1, 32 * 3);

	PixelShader->UnbindBuffers(RHICmdList);
}
