// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "StandaloneRendererPrivate.h"

#include "Windows/D3D/SlateD3DShaders.h"
#include "Windows/D3D/SlateD3DRenderer.h"
#include "Windows/D3D/SlateD3DRenderingPolicy.h"

static void CompileShader( const FString& Filename, const FString& EntryPoint, const FString& ShaderModel, TRefCountPtr<ID3DBlob>& OutBlob )
{
	uint32 ShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if UE_BUILD_DEBUG
	ShaderFlags |= D3D10_SHADER_DEBUG;
#else
	ShaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	TRefCountPtr<ID3DBlob> ErrorBlob;
	HRESULT Hr = D3DX11CompileFromFile( *Filename, NULL, NULL, TCHAR_TO_ANSI(*EntryPoint), TCHAR_TO_ANSI(*ShaderModel), ShaderFlags, 0, NULL, OutBlob.GetInitReference(), ErrorBlob.GetInitReference(), NULL );
	if( FAILED(Hr) )
	{
		if( ErrorBlob.GetReference() )
		{
			checkf(0, ANSI_TO_TCHAR(ErrorBlob->GetBufferPointer()));
		}
		else
		{
			checkf(0, TEXT("D3DX11CompileFromFile failed, no error text provided"));
		}
	}
}

static void GetShaderBindings( TRefCountPtr<ID3D11ShaderReflection> Reflector, FSlateD3DShaderBindings& OutBindings )
{
	D3D11_SHADER_DESC ShaderDesc;
	Reflector->GetDesc( &ShaderDesc );

	for( uint32 I = 0; I < ShaderDesc.BoundResources; ++I )
	{
		D3D11_SHADER_INPUT_BIND_DESC Desc;
		Reflector->GetResourceBindingDesc( I, &Desc );

		FSlateD3DShaderParameter* Param = FSlateShaderParameterMap::Get().Find( Desc.Name );
		if( Param )
		{
			if( Desc.Type == D3D_SIT_TEXTURE )
			{
				OutBindings.ResourceViews.Add( (TSlateD3DTypedShaderParameter<ID3D11ShaderResourceView>*)Param );
			}
			else if(  Desc.Type == D3D_SIT_CBUFFER  )
			{
				OutBindings.ConstantBuffers.Add( (TSlateD3DTypedShaderParameter<ID3D11Buffer>*)Param );
			}
			else if( Desc.Type == D3D_SIT_SAMPLER )
			{
				OutBindings.SamplerStates.Add( (TSlateD3DTypedShaderParameter<ID3D11SamplerState>*)Param );
			}
			else
			{
				// unhandled param type
				check(0);
			}
		}
		else
		{
			// not registered
			check(0);
		}

	}
}

void FSlateD3DVS::Create( const FString& Filename, const FString& EntryPoint, const FString& ShaderModel, D3D11_INPUT_ELEMENT_DESC* VertexLayout, uint32 VertexLayoutCount )
{
	TRefCountPtr<ID3DBlob> Blob;
	CompileShader( Filename, EntryPoint, ShaderModel, Blob);

	HRESULT Hr = GD3DDevice->CreateVertexShader( Blob->GetBufferPointer(), Blob->GetBufferSize(), NULL, VertexShader.GetInitReference() );
	checkf( SUCCEEDED(Hr), TEXT("D3D11 Error Result %X"), Hr );

	Hr = GD3DDevice->CreateInputLayout( VertexLayout, VertexLayoutCount, Blob->GetBufferPointer(), Blob->GetBufferSize(), InputLayout.GetInitReference() );
	checkf( SUCCEEDED(Hr), TEXT("D3D11 Error Result %X"), Hr );

	TRefCountPtr<ID3D11ShaderReflection> Reflector;
	Hr = D3DReflect( Blob->GetBufferPointer(), Blob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)Reflector.GetInitReference() );
	checkf( SUCCEEDED(Hr), TEXT("D3D11 Error Result %X"), Hr );

	GetShaderBindings( Reflector, ShaderBindings );
}


void FSlateD3DVS::BindShader()
{
	GD3DDeviceContext->IASetInputLayout( InputLayout );
	GD3DDeviceContext->VSSetShader( VertexShader, NULL, 0 );
}

void FSlateD3DVS::BindParameters()
{
	UpdateParameters();

	int32 NumViews = ShaderBindings.ResourceViews.Num();
	if( NumViews > 0 )
	{
		ID3D11ShaderResourceView** const Views = new ID3D11ShaderResourceView*[ NumViews ];
		for( int32 I = 0; I < NumViews; ++I )
		{
			Views[I] = ShaderBindings.ResourceViews[I]->GetParameter().GetReference();
		}

		GD3DDeviceContext->VSSetShaderResources(0, NumViews, Views);

		delete[] Views;
	}

	if( ShaderBindings.ConstantBuffers.Num() > 0 )
	{
		const uint32 BufferCount = ShaderBindings.ConstantBuffers.Num();
		ID3D11Buffer** const ConstantBuffers = new ID3D11Buffer*[ BufferCount ];
		for( uint32 I = 0; I < BufferCount; ++I )
		{
			ConstantBuffers[I] = ShaderBindings.ConstantBuffers[I]->GetParameter().GetReference();
		}

		GD3DDeviceContext->VSSetConstantBuffers(0, ShaderBindings.ConstantBuffers.Num(), ConstantBuffers);

		delete[] ConstantBuffers;
	}	
}
void FSlateD3DGeometryShader::Create( const FString& Filename, const FString& EntryPoint, const FString& ShaderModel )
{
	TRefCountPtr<ID3DBlob> Blob;
	CompileShader( Filename, EntryPoint, ShaderModel, Blob);

	HRESULT Hr = GD3DDevice->CreateGeometryShader( Blob->GetBufferPointer(), Blob->GetBufferSize(), NULL, GeometryShader.GetInitReference() );
	checkf( SUCCEEDED(Hr), TEXT("D3D11 Error Result %X"), Hr );


	TRefCountPtr<ID3D11ShaderReflection> Reflector;
	Hr = D3DReflect( Blob->GetBufferPointer(), Blob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)Reflector.GetInitReference() );
	checkf( SUCCEEDED(Hr), TEXT("D3D11 Error Result %X"), Hr );

	GetShaderBindings( Reflector, ShaderBindings );
}


void FSlateD3DGeometryShader::BindShader()
{
	GD3DDeviceContext->GSSetShader( GeometryShader, NULL, 0 );
}

void FSlateD3DGeometryShader::BindParameters()
{
	UpdateParameters();

	if( ShaderBindings.ConstantBuffers.Num() )
	{
		const uint32 BufferCount = ShaderBindings.ConstantBuffers.Num();
		ID3D11Buffer** const ConstantBuffers = new ID3D11Buffer*[ BufferCount ];
		for( uint32 I = 0; I < BufferCount; ++I )
		{
			ConstantBuffers[I] = ShaderBindings.ConstantBuffers[I]->GetParameter().GetReference();
		}

		GD3DDeviceContext->GSSetConstantBuffers(0, ShaderBindings.ConstantBuffers.Num(), ConstantBuffers);

		delete[] ConstantBuffers;
	}	
}

void FSlateD3DPS::Create( const FString& Filename, const FString& EntryPoint, const FString& ShaderModel )
{
	TRefCountPtr<ID3DBlob> Blob;
	CompileShader( Filename, EntryPoint, ShaderModel, Blob);

	HRESULT Hr = GD3DDevice->CreatePixelShader( Blob->GetBufferPointer(), Blob->GetBufferSize(), NULL, PixelShader.GetInitReference() );
	checkf( SUCCEEDED(Hr), TEXT("D3D11 Error Result %X"), Hr );


	TRefCountPtr<ID3D11ShaderReflection> Reflector;
	Hr = D3DReflect( Blob->GetBufferPointer(), Blob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)Reflector.GetInitReference() );
	checkf( SUCCEEDED(Hr), TEXT("D3D11 Error Result %X"), Hr );

	GetShaderBindings( Reflector, ShaderBindings );
}


void FSlateD3DPS::BindShader()
{
	GD3DDeviceContext->PSSetShader( PixelShader, NULL, 0 );
}

void FSlateD3DPS::BindParameters()
{
	UpdateParameters();

	int32 NumViews = ShaderBindings.ResourceViews.Num();

	if( NumViews )
	{
		ID3D11ShaderResourceView** const Views = new ID3D11ShaderResourceView*[ NumViews ];
		for( int32 I = 0; I < NumViews; ++I )
		{
			Views[I] = ShaderBindings.ResourceViews[I]->GetParameter().GetReference();
		}

		GD3DDeviceContext->PSSetShaderResources(0, NumViews, Views);

		delete[] Views;
	}

	int32 NumBuffers = ShaderBindings.ConstantBuffers.Num();

	if( NumBuffers )
	{
		ID3D11Buffer** const ConstantBuffers = new ID3D11Buffer*[ NumBuffers ];
		for( int32 I = 0; I < NumBuffers; ++I )
		{
			ConstantBuffers[I] = ShaderBindings.ConstantBuffers[I]->GetParameter().GetReference();
		}

		GD3DDeviceContext->PSSetConstantBuffers(0, NumBuffers, ConstantBuffers);

		delete[] ConstantBuffers;
	}	

	if( ShaderBindings.SamplerStates.Num() )
	{
		const uint32 StateCount = ShaderBindings.SamplerStates.Num();
		ID3D11SamplerState** const SamplerStates = new ID3D11SamplerState*[ StateCount ];
		for( uint32 I = 0; I < StateCount; ++I )
		{
			SamplerStates[I] = ShaderBindings.SamplerStates[I]->GetParameter().GetReference();
		}

		GD3DDeviceContext->PSSetSamplers(0, ShaderBindings.SamplerStates.Num(), SamplerStates);

		delete[] SamplerStates;
	}	
}


FSlateDefaultVS::FSlateDefaultVS()
{
	Constants = &FSlateShaderParameterMap::Get().RegisterParameter<ID3D11Buffer>( "PerElementVSConstants" );

	ConstantBuffer.Create();

	D3D11_INPUT_ELEMENT_DESC Layout[] = 
	{
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 0,								D3D11_INPUT_PER_VERTEX_DATA,	0 },
		{ "TEXCOORD",	3, DXGI_FORMAT_R32G32_FLOAT,		0, D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0 },
		{ "POSITION",	0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0 },
		{ "TEXCOORD",	1, DXGI_FORMAT_R32G32_FLOAT,		0, D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0 },
		{ "TEXCOORD",	2, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0 },
		{ "COLOR",		0, DXGI_FORMAT_B8G8R8A8_UNORM,		0, D3D11_APPEND_ALIGNED_ELEMENT,	D3D11_INPUT_PER_VERTEX_DATA,	0 },
	};

	Create( FString::Printf( TEXT("%sShaders/StandaloneRenderer/D3D/SlateDefaultVertexShader.hlsl"), *FPaths::EngineDir() ), TEXT("Main"), TEXT("vs_4_0"), Layout, ARRAY_COUNT(Layout) );

}

void FSlateDefaultVS::SetViewProjection( const FMatrix& ViewProjectionMatrix )
{
	ConstantBuffer.GetBufferData().ViewProjection = ViewProjectionMatrix;
}

void FSlateDefaultVS::SetShaderParams( const FVector4& InShaderParams )
{
	ConstantBuffer.GetBufferData().VertexShaderParams = InShaderParams;
}

void FSlateDefaultVS::UpdateParameters()
{

	ConstantBuffer.UpdateBuffer();

	// Set the constant parameter to use our constant buffer
	Constants->SetParameter( ConstantBuffer.GetResource() );
}


FSlateDefaultPS::FSlateDefaultPS()
{
	Texture = &FSlateShaderParameterMap::Get().RegisterParameter<ID3D11ShaderResourceView>( "ElementTexture" );
	TextureSampler = &FSlateShaderParameterMap::Get().RegisterParameter<ID3D11SamplerState>( "ElementTextureSampler" );
	PerFrameCBufferParam = &FSlateShaderParameterMap::Get().RegisterParameter<ID3D11Buffer>("PerFramePSConstants");
	PerElementCBufferParam = &FSlateShaderParameterMap::Get().RegisterParameter<ID3D11Buffer>("PerElementPSConstants");

	PerFrameConstants.Create();
	PerElementConstants.Create();

	PerFrameConstants.GetBufferData().GammaValues = FVector2D(1, 1 / 2.2f);

	PerFrameCBufferParam->SetParameter(PerFrameConstants.GetResource());

	// Set the constant parameter to use our constant buffer
	// @todo: If we go back to multiple pixel shaders this likely has be called more frequently
	PerElementCBufferParam->SetParameter(PerElementConstants.GetResource());

	Create( FString::Printf( TEXT("%sShaders/StandaloneRenderer/D3D/SlateElementPixelShader.hlsl"), *FPaths::EngineDir() ), TEXT("Main"), TEXT("ps_4_0") );
}

void FSlateDefaultPS::SetShaderType( uint32 InShaderType )
{
	PerElementConstants.GetBufferData().ShaderType = InShaderType;
}

void FSlateDefaultPS::SetDrawEffects( uint32 InDrawEffects )
{
	PerElementConstants.GetBufferData().DrawEffects = InDrawEffects;
}

void FSlateDefaultPS::SetShaderParams( const FVector4& InShaderParams )
{
	PerElementConstants.GetBufferData().ShaderParams = InShaderParams;
}

void FSlateDefaultPS::SetGammaValues(const FVector2D& InGammaValues)
{
	PerFrameConstants.GetBufferData().GammaValues = InGammaValues;
}

void FSlateDefaultPS::UpdateParameters()
{
	PerFrameConstants.UpdateBuffer();
	PerElementConstants.UpdateBuffer();

	TextureSampler->SetParameter( SamplerState );
}
