// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "GlobalShader.h"
#include "ShaderParameters.h"

struct FDummyResolveParameter {};

class FResolveDepthPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FResolveDepthPS, Global, UTILITYSHADERS_API);
public:
	
	typedef FDummyResolveParameter FParameter;
	
	static bool ShouldCache(EShaderPlatform Platform) { return Platform == SP_PCD3D_SM5; }
	
	FResolveDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
	{
		UnresolvedSurface.Bind(Initializer.ParameterMap,TEXT("UnresolvedSurface"), SPF_Mandatory);
	}
	FResolveDepthPS() {}
	
	void SetParameters(FRHICommandList& RHICmdList, FParameter)
	{
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << UnresolvedSurface;
		return bShaderHasOutdatedParameters;
	}
	
	FShaderResourceParameter UnresolvedSurface;
};


class FResolveDepthNonMSPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FResolveDepthNonMSPS, Global, UTILITYSHADERS_API);
public:
	
	typedef FDummyResolveParameter FParameter;
	
	static bool ShouldCache(EShaderPlatform Platform) { return GetMaxSupportedFeatureLevel(Platform) <= ERHIFeatureLevel::SM4; }
	
	FResolveDepthNonMSPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
	{
		UnresolvedSurface.Bind(Initializer.ParameterMap,TEXT("UnresolvedSurfaceNonMS"), SPF_Mandatory);
	}
	FResolveDepthNonMSPS() {}
	
	void SetParameters(FRHICommandList& RHICmdList, FParameter)
	{
	}
	
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << UnresolvedSurface;
		return bShaderHasOutdatedParameters;
	}
	
	FShaderResourceParameter UnresolvedSurface;
};

class FResolveSingleSamplePS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FResolveSingleSamplePS, Global, UTILITYSHADERS_API);
public:
	
	typedef uint32 FParameter;
	
	static bool ShouldCache(EShaderPlatform Platform) { return Platform == SP_PCD3D_SM5; }
	
	FResolveSingleSamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
	{
		UnresolvedSurface.Bind(Initializer.ParameterMap,TEXT("UnresolvedSurface"), SPF_Mandatory);
		SingleSampleIndex.Bind(Initializer.ParameterMap,TEXT("SingleSampleIndex"), SPF_Mandatory);
	}
	FResolveSingleSamplePS() {}
	
	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, uint32 SingleSampleIndexValue);
	
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << UnresolvedSurface;
		Ar << SingleSampleIndex;
		return bShaderHasOutdatedParameters;
	}
	
	FShaderResourceParameter UnresolvedSurface;
	FShaderParameter SingleSampleIndex;
};

/**
 * A vertex shader for rendering a textured screen element.
 */
class FResolveVS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FResolveVS, Global, UTILITYSHADERS_API);
public:
	
	static bool ShouldCache(EShaderPlatform Platform) { return true; }
	
	FResolveVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
	FGlobalShader(Initializer)
	{}
	FResolveVS() {}
};
