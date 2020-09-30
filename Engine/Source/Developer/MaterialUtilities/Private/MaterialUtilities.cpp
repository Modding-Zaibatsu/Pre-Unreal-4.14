// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "MaterialUtilitiesPrivatePCH.h"

#include "Runtime/Engine/Classes/Engine/World.h"
#include "Runtime/Engine/Classes/Materials/MaterialInterface.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionConstant.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionConstant4Vector.h"
#include "Runtime/Engine/Classes/Materials/MaterialExpressionMultiply.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "Runtime/Engine/Classes/Engine/Texture2D.h"
#include "Runtime/Engine/Classes/Engine/TextureCube.h"
#include "Runtime/Engine/Public/TileRendering.h"
#include "Runtime/Engine/Public/EngineModule.h"
#include "Runtime/Engine/Public/ImageUtils.h"
#include "Runtime/Engine/Public/CanvasTypes.h"
#include "Runtime/Engine/Public/MaterialCompiler.h"
#include "Runtime/Engine/Classes/Engine/TextureLODSettings.h"
#include "Runtime/Engine/Classes/DeviceProfiles/DeviceProfileManager.h"
#include "Runtime/Engine/Classes/Materials/MaterialParameterCollection.h" 
#include "RendererInterface.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "MeshUtilities.h"
#include "MeshRendering.h"
#include "MeshMergeData.h"
#include "Runtime/Engine/Classes/Engine/StaticMesh.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#include "ObjectTools.h"
#include "Tests/AutomationEditorCommon.h"
#endif // WITH_EDITOR

IMPLEMENT_MODULE(FMaterialUtilities, MaterialUtilities);

DEFINE_LOG_CATEGORY_STATIC(LogMaterialUtilities, Log, All);

bool FMaterialUtilities::CurrentlyRendering = false;
TArray<UTextureRenderTarget2D*> FMaterialUtilities::RenderTargetPool;

void FMaterialUtilities::StartupModule()
{
	FCoreUObjectDelegates::PreGarbageCollect.AddRaw(this, &FMaterialUtilities::OnPreGarbageCollect);
}

void FMaterialUtilities::ShutdownModule()
{
	FCoreUObjectDelegates::PreGarbageCollect.RemoveAll(this);
	ClearRenderTargetPool();
}

void FMaterialUtilities::OnPreGarbageCollect()
{
	ClearRenderTargetPool();
}


/*------------------------------------------------------------------------------
	Helper classes for render material to texture
------------------------------------------------------------------------------*/
struct FExportMaterialCompiler : public FProxyMaterialCompiler
{
	FExportMaterialCompiler(FMaterialCompiler* InCompiler) :
		FProxyMaterialCompiler(InCompiler)
	{}

	// gets value stored by SetMaterialProperty()
	virtual EShaderFrequency GetCurrentShaderFrequency() const override
	{
		// not used by Lightmass
		return SF_Pixel;
	}

	virtual EMaterialShadingModel GetMaterialShadingModel() const override
	{
		// not used by Lightmass
		return MSM_MAX;
	}

	virtual int32 WorldPosition(EWorldPositionIncludedOffsets WorldPositionIncludedOffsets) override
	{
#if WITH_EDITOR
		return Compiler->MaterialBakingWorldPosition();
#else
		return Compiler->WorldPosition(WorldPositionIncludedOffsets);
#endif
	}

	virtual int32 ObjectWorldPosition() override
	{
		return Compiler->ObjectWorldPosition();
	}

	virtual int32 DistanceCullFade() override
	{
		return Compiler->Constant(1.0f);
	}

	virtual int32 ActorWorldPosition() override
	{
		return Compiler->ActorWorldPosition();
	}

	virtual int32 ParticleRelativeTime() override
	{
		return Compiler->Constant(0.0f);
	}

	virtual int32 ParticleMotionBlurFade() override
	{
		return Compiler->Constant(1.0f);
	}
	
	virtual int32 PixelNormalWS() override
	{
		// Current returning vertex normal since pixel normal will contain incorrect data (normal calculated from uv data used as vertex positions to render out the material)
		return Compiler->VertexNormal();
	}

	virtual int32 ParticleRandom() override
	{
		return Compiler->Constant(0.0f);
	}

	virtual int32 ParticleDirection() override
	{
		return Compiler->Constant3(0.0f, 0.0f, 0.0f);
	}

	virtual int32 ParticleSpeed() override
	{
		return Compiler->Constant(0.0f);
	}
	
	virtual int32 ParticleSize() override
	{
		return Compiler->Constant2(0.0f,0.0f);
	}

	virtual int32 ObjectRadius() override
	{
		return Compiler->Constant(500);
	}

	virtual int32 ObjectBounds() override
	{
		return Compiler->ObjectBounds();
	}

	virtual int32 CameraVector() override
	{
		return Compiler->Constant3(0.0f, 0.0f, 1.0f);
	}
	
	virtual int32 ReflectionAboutCustomWorldNormal(int32 CustomWorldNormal, int32 bNormalizeCustomWorldNormal) override
	{
		return Compiler->ReflectionAboutCustomWorldNormal(CustomWorldNormal, bNormalizeCustomWorldNormal);
	}

	virtual int32 VertexColor() override
	{
		return Compiler->VertexColor(); 
	}

	virtual int32 LightVector() override
	{
		return Compiler->LightVector();
	}

	virtual int32 ReflectionVector() override
	{
		return Compiler->ReflectionVector();
	}

	virtual int32 AtmosphericFogColor(int32 WorldPosition) override
	{
		return INDEX_NONE;
	}

	virtual int32 PrecomputedAOMask() override 
	{ 
		return Compiler->PrecomputedAOMask();
	}

#if WITH_EDITOR
	virtual int32 MaterialBakingWorldPosition() override
	{
		return Compiler->MaterialBakingWorldPosition();
	}
#endif

	virtual int32 AccessCollectionParameter(UMaterialParameterCollection* ParameterCollection, int32 ParameterIndex, int32 ComponentIndex) override
	{
		if (!ParameterCollection || ParameterIndex == -1)
		{
			return INDEX_NONE;
		}

		// Collect names of all parameters
		TArray<FName> ParameterNames;
		ParameterCollection->GetParameterNames(ParameterNames, /*bVectorParameters=*/ false);
		int32 NumScalarParameters = ParameterNames.Num();
		ParameterCollection->GetParameterNames(ParameterNames, /*bVectorParameters=*/ true);

		// Find a parameter corresponding to ParameterIndex/ComponentIndex pair
		int32 Index;
		for (Index = 0; Index < ParameterNames.Num(); Index++)
		{
			FGuid ParameterId = ParameterCollection->GetParameterId(ParameterNames[Index]);
			int32 CheckParameterIndex, CheckComponentIndex;
			ParameterCollection->GetParameterIndex(ParameterId, CheckParameterIndex, CheckComponentIndex);
			if (CheckParameterIndex == ParameterIndex && CheckComponentIndex == ComponentIndex)
			{
				// Found
				break;
			}
		}
		if (Index >= ParameterNames.Num())
		{
			// Not found, should not happen
			return INDEX_NONE;
		}

		// Create code for parameter
		if (Index < NumScalarParameters)
		{
			const FCollectionScalarParameter* ScalarParameter = ParameterCollection->GetScalarParameterByName(ParameterNames[Index]);
			check(ScalarParameter);
			return Constant(ScalarParameter->DefaultValue);
		}
		else
		{
			const FCollectionVectorParameter* VectorParameter = ParameterCollection->GetVectorParameterByName(ParameterNames[Index]);
			check(VectorParameter);
			const FLinearColor& Color = VectorParameter->DefaultValue;
			return Constant4(Color.R, Color.G, Color.B, Color.A);
		}
	}
	
	virtual int32 LightmassReplace(int32 Realtime, int32 Lightmass) override { return Realtime; }
	virtual int32 MaterialProxyReplace(int32 Realtime, int32 MaterialProxy) override { return MaterialProxy; }
};


class FExportMaterialProxy : public FMaterial, public FMaterialRenderProxy
{
public:
	FExportMaterialProxy()
		: FMaterial()
	{
		SetQualityLevelProperties(EMaterialQualityLevel::High, false, GMaxRHIFeatureLevel);
	}

	FExportMaterialProxy(UMaterialInterface* InMaterialInterface, EMaterialProperty InPropertyToCompile)
		: FMaterial()
		, MaterialInterface(InMaterialInterface)
		, PropertyToCompile(InPropertyToCompile)
	{
		SetQualityLevelProperties(EMaterialQualityLevel::High, false, GMaxRHIFeatureLevel);
		Material = InMaterialInterface->GetMaterial();
		Material->AppendReferencedTextures(ReferencedTextures);
		FPlatformMisc::CreateGuid(Id);

		FMaterialResource* Resource = InMaterialInterface->GetMaterialResource(GMaxRHIFeatureLevel);

		FMaterialShaderMapId ResourceId;
		Resource->GetShaderMapId(GMaxRHIShaderPlatform, ResourceId);

		{
			TArray<FShaderType*> ShaderTypes;
			TArray<FVertexFactoryType*> VFTypes;
			TArray<const FShaderPipelineType*> ShaderPipelineTypes;
			GetDependentShaderAndVFTypes(GMaxRHIShaderPlatform, ShaderTypes, ShaderPipelineTypes, VFTypes);

			// Overwrite the shader map Id's dependencies with ones that came from the FMaterial actually being compiled (this)
			// This is necessary as we change FMaterial attributes like GetShadingModel(), which factor into the ShouldCache functions that determine dependent shader types
			ResourceId.SetShaderDependencies(ShaderTypes, ShaderPipelineTypes, VFTypes);
		}

		// Override with a special usage so we won't re-use the shader map used by the material for rendering
		switch (InPropertyToCompile)
		{
		case MP_BaseColor: ResourceId.Usage = EMaterialShaderMapUsage::MaterialExportBaseColor; break;
		case MP_Specular: ResourceId.Usage = EMaterialShaderMapUsage::MaterialExportSpecular; break;
		case MP_Normal: ResourceId.Usage = EMaterialShaderMapUsage::MaterialExportNormal; break;
		case MP_Metallic: ResourceId.Usage = EMaterialShaderMapUsage::MaterialExportMetallic; break;
		case MP_Roughness: ResourceId.Usage = EMaterialShaderMapUsage::MaterialExportRoughness; break;
		case MP_AmbientOcclusion: ResourceId.Usage = EMaterialShaderMapUsage::MaterialExportAO; break;
		case MP_EmissiveColor: ResourceId.Usage = EMaterialShaderMapUsage::MaterialExportEmissive; break;
		case MP_Opacity: ResourceId.Usage = EMaterialShaderMapUsage::MaterialExportOpacity; break;
		case MP_SubsurfaceColor: ResourceId.Usage = EMaterialShaderMapUsage::MaterialExportSubSurfaceColor; break;
		default:
			ensureMsgf(false, TEXT("ExportMaterial has no usage for property %i.  Will likely reuse the normal rendering shader and crash later with a parameter mismatch"), (int32)InPropertyToCompile);
			break;
		};
		
		CacheShaders(ResourceId, GMaxRHIShaderPlatform, true);
	}

	/** This override is required otherwise the shaders aren't ready for use when the surface is rendered resulting in a blank image */
	virtual bool RequiresSynchronousCompilation() const override { return true; };

	/**
	* Should the shader for this material with the given platform, shader type and vertex 
	* factory type combination be compiled
	*
	* @param Platform		The platform currently being compiled for
	* @param ShaderType	Which shader is being compiled
	* @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	*
	* @return true if the shader should be compiled
	*/
	virtual bool ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const override
	{
		// Always cache - decreases performance but avoids missing shaders during exports.
		return true;
	}

	virtual const TArray<UTexture*>& GetReferencedTextures() const override
	{
		return ReferencedTextures;
	}

	////////////////
	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const override
	{
		if(GetRenderingThreadShaderMap())
		{
			return this;
		}
		else
		{
			return UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false)->GetMaterial(FeatureLevel);
		}
	}

	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const override
	{
		return MaterialInterface->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context);
	}

	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const override
	{
		return MaterialInterface->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context);
	}

	virtual bool GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const override
	{
		return MaterialInterface->GetRenderProxy(0)->GetTextureValue(ParameterName,OutValue,Context);
	}

	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual int32 CompilePropertyAndSetMaterialProperty(EMaterialProperty Property, FMaterialCompiler* Compiler, EShaderFrequency OverrideShaderFrequency, bool bUsePreviousFrameTime) const override
	{
		// needs to be called in this function!!
		Compiler->SetMaterialProperty(Property, OverrideShaderFrequency, bUsePreviousFrameTime);

		int32 Ret = CompilePropertyAndSetMaterialPropertyWithoutCast(Property, Compiler);

		return Compiler->ForceCast(Ret, GetMaterialPropertyType(Property));
	}

	/** helper for CompilePropertyAndSetMaterialProperty() */
	int32 CompilePropertyAndSetMaterialPropertyWithoutCast(EMaterialProperty Property, FMaterialCompiler* Compiler) const
	{
		/*TScopedPointer<FMaterialCompiler> CompilerProxyHolder;
		if (CompilerReplacer != nullptr)
		{
			CompilerProxyHolder = CompilerReplacer(Compiler);
			Compiler = CompilerProxyHolder;
		}*/

		if (Property == MP_EmissiveColor)
		{
			UMaterial* ProxyMaterial = MaterialInterface->GetMaterial();
			check(ProxyMaterial);
			EBlendMode BlendMode = MaterialInterface->GetBlendMode();
			EMaterialShadingModel ShadingModel = MaterialInterface->GetShadingModel();
			FExportMaterialCompiler ProxyCompiler(Compiler);
									
			switch (PropertyToCompile)
			{
			case MP_EmissiveColor:
				// Emissive is ALWAYS returned...
				return Compiler->ForceCast(MaterialInterface->CompileProperty(&ProxyCompiler,MP_EmissiveColor),MCT_Float3,true,true);
			case MP_BaseColor:
				// Only return for Opaque and Masked...
				if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
				{
					return Compiler->ForceCast(MaterialInterface->CompileProperty(&ProxyCompiler, MP_BaseColor),MCT_Float3,true,true);
				}
				break;
			case MP_Specular: 
			case MP_Roughness:
			case MP_Metallic:
			case MP_AmbientOcclusion:
				// Only return for Opaque and Masked...
				if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
				{
					return Compiler->ForceCast(MaterialInterface->CompileProperty(&ProxyCompiler, PropertyToCompile),MCT_Float,true,true);
				}
				break;
			case MP_Normal:
				// Only return for Opaque and Masked...
				if (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
				{
					return Compiler->ForceCast( 
						Compiler->Add( 
							Compiler->Mul(MaterialInterface->CompileProperty(&ProxyCompiler, MP_Normal), Compiler->Constant(0.5f)), // [-1,1] * 0.5
							Compiler->Constant(0.5f)), // [-0.5,0.5] + 0.5
						MCT_Float3, true, true );
				}
				break;
			default:
				return Compiler->Constant(1.0f);
			}
	
			return Compiler->Constant(0.0f);
		}
		else if (Property == MP_WorldPositionOffset)
		{
			//This property MUST return 0 as a default or during the process of rendering textures out for lightmass to use, pixels will be off by 1.
			return Compiler->Constant(0.0f);
		}
		else if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
		{
			// Pass through customized UVs
			return MaterialInterface->CompileProperty(Compiler, Property);
		}
		else
		{
			return Compiler->Constant(1.0f);
		}
	}

	virtual FString GetMaterialUsageDescription() const override
	{
		return FString::Printf(TEXT("FExportMaterialRenderer %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL"));
	}
	virtual int32 GetMaterialDomain() const override
	{
		if (Material)
		{
			return Material->MaterialDomain;
		}
		return MD_Surface;
	}
	virtual bool IsTwoSided() const  override
	{ 
		if (MaterialInterface)
		{
			return MaterialInterface->IsTwoSided();
		}
		return false;
	}
	virtual bool IsDitheredLODTransition() const  override
	{ 
		if (MaterialInterface)
		{
			return MaterialInterface->IsDitheredLODTransition();
		}
		return false;
	}
	virtual bool IsLightFunction() const override
	{
		if (Material)
		{
			return (Material->MaterialDomain == MD_LightFunction);
		}
		return false;
	}
	virtual bool IsDeferredDecal() const override
	{
		return Material && Material->MaterialDomain == MD_DeferredDecal;
	}
	virtual bool IsSpecialEngineMaterial() const override
	{
		if (Material)
		{
			return (Material->bUsedAsSpecialEngineMaterial == 1);
		}
		return false;
	}
	virtual bool IsWireframe() const override
	{
		if (Material)
		{
			return (Material->Wireframe == 1);
		}
		return false;
	}
	virtual bool IsMasked() const override								{ return false; }
	virtual enum EBlendMode GetBlendMode() const override				{ return BLEND_Opaque; }
	virtual enum EMaterialShadingModel GetShadingModel() const override	{ return MSM_Unlit; }
	virtual float GetOpacityMaskClipValue() const override				{ return 0.5f; }
	virtual FString GetFriendlyName() const override { return FString::Printf(TEXT("FExportMaterialRenderer %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); }
	/**
	* Should shaders compiled for this material be saved to disk?
	*/
	virtual bool IsPersistent() const override { return false; }
	virtual FGuid GetMaterialId() const override { return Id; }

	const UMaterialInterface* GetMaterialInterface() const
	{
		return MaterialInterface;
	}

	friend FArchive& operator<< ( FArchive& Ar, FExportMaterialProxy& V )
	{
		return Ar << V.MaterialInterface;
	}

	/**
	* Iterate through all textures used by the material and return the maximum texture resolution used
	* (ideally this could be made dependent of the material property)
	*
	* @param MaterialInterface The material to scan for texture size
	*
	* @return Size (width and height)
	*/
	FIntPoint FindMaxTextureSize(UMaterialInterface* InMaterialInterface, FIntPoint MinimumSize = FIntPoint(1, 1)) const
	{
		// static lod settings so that we only initialize them once
		UTextureLODSettings* GameTextureLODSettings = UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings();

		TArray<UTexture*> MaterialTextures;

		InMaterialInterface->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num, false, GMaxRHIFeatureLevel, false);

		// find the largest texture in the list (applying it's LOD bias)
		FIntPoint MaxSize = MinimumSize;
		for (int32 TexIndex = 0; TexIndex < MaterialTextures.Num(); TexIndex++)
		{
			UTexture* Texture = MaterialTextures[TexIndex];

			if (Texture == NULL)
			{
				continue;
			}

			// get the max size of the texture
			FIntPoint LocalSize(0, 0);
			if (Texture->IsA(UTexture2D::StaticClass()))
			{
				UTexture2D* Tex2D = (UTexture2D*)Texture;
				LocalSize = FIntPoint(Tex2D->GetSizeX(), Tex2D->GetSizeY());
			}
			else if (Texture->IsA(UTextureCube::StaticClass()))
			{
				UTextureCube* TexCube = (UTextureCube*)Texture;
				LocalSize = FIntPoint(TexCube->GetSizeX(), TexCube->GetSizeY());
			}

			int32 LocalBias = GameTextureLODSettings->CalculateLODBias(Texture);

			// bias the texture size based on LOD group
			FIntPoint BiasedLocalSize(LocalSize.X >> LocalBias, LocalSize.Y >> LocalBias);

			MaxSize.X = FMath::Max(BiasedLocalSize.X, MaxSize.X);
			MaxSize.Y = FMath::Max(BiasedLocalSize.Y, MaxSize.Y);
		}

		return MaxSize;
	}

	static bool WillFillData(EBlendMode InBlendMode, EMaterialProperty InMaterialProperty)
	{
		if (InMaterialProperty == MP_EmissiveColor)
		{
			return true;
		}

		switch (InBlendMode)
		{
		case BLEND_Opaque:
			{
				switch (InMaterialProperty)
				{
				case MP_BaseColor:		return true;
				case MP_Specular:		return true;
				case MP_Normal:			return true;
				case MP_Metallic:		return true;
				case MP_Roughness:		return true;
				case MP_AmbientOcclusion:		return true;
				}
			}
			break;
		}
		return false;
	}

private:
	/** The material interface for this proxy */
	UMaterialInterface* MaterialInterface;
	UMaterial* Material;	
	TArray<UTexture*> ReferencedTextures;
	/** The property to compile for rendering the sample */
	EMaterialProperty PropertyToCompile;
	FGuid Id;
};

static void RenderSceneToTexture(
		FSceneInterface* Scene,
		const FName& VisualizationMode, 
		const FVector& ViewOrigin,
		const FMatrix& ViewRotationMatrix, 
		const FMatrix& ProjectionMatrix,  
		const TSet<FPrimitiveComponentId>& HiddenPrimitives, 
		FIntPoint TargetSize,
		float TargetGamma,
		TArray<FColor>& OutSamples)
{
	auto RenderTargetTexture = NewObject<UTextureRenderTarget2D>();
	check(RenderTargetTexture);
	RenderTargetTexture->AddToRoot();
	RenderTargetTexture->ClearColor = FLinearColor::Transparent;
	RenderTargetTexture->TargetGamma = TargetGamma;
	RenderTargetTexture->InitCustomFormat(TargetSize.X, TargetSize.Y, PF_FloatRGBA, false);
	FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();

	FSceneViewFamilyContext ViewFamily(
		FSceneViewFamily::ConstructionValues(RenderTargetResource, Scene, FEngineShowFlags(ESFIM_Game))
			.SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime)
		);

	// To enable visualization mode
	ViewFamily.EngineShowFlags.SetPostProcessing(true);
	ViewFamily.EngineShowFlags.SetVisualizeBuffer(true);
	ViewFamily.EngineShowFlags.SetTonemapper(false);

	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.SetViewRectangle(FIntRect(0, 0, TargetSize.X, TargetSize.Y));
	ViewInitOptions.ViewFamily = &ViewFamily;
	ViewInitOptions.HiddenPrimitives = HiddenPrimitives;
	ViewInitOptions.ViewOrigin = ViewOrigin;
	ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
	ViewInitOptions.ProjectionMatrix = ProjectionMatrix;
		
	FSceneView* NewView = new FSceneView(ViewInitOptions);
	NewView->CurrentBufferVisualizationMode = VisualizationMode;
	ViewFamily.Views.Add(NewView);
					
	FCanvas Canvas(RenderTargetResource, NULL, FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime, Scene->GetFeatureLevel());
	Canvas.Clear(FLinearColor::Transparent);
	GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);

	// Copy the contents of the remote texture to system memory
	OutSamples.SetNumUninitialized(TargetSize.X*TargetSize.Y);
	FReadSurfaceDataFlags ReadSurfaceDataFlags;
	ReadSurfaceDataFlags.SetLinearToGamma(false);
	RenderTargetResource->ReadPixelsPtr(OutSamples.GetData(), ReadSurfaceDataFlags, FIntRect(0, 0, TargetSize.X, TargetSize.Y));
	FlushRenderingCommands();
					
	RenderTargetTexture->RemoveFromRoot();
	RenderTargetTexture = nullptr;
}



bool FMaterialUtilities::SupportsExport(EBlendMode InBlendMode, EMaterialProperty InMaterialProperty)
{
	return FExportMaterialProxy::WillFillData(InBlendMode, InMaterialProperty);
}

bool FMaterialUtilities::ExportMaterialProperty(UWorld* InWorld, UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, UTextureRenderTarget2D* InRenderTarget, TArray<FColor>& OutBMP)
{
	TScopedPointer<FExportMaterialProxy> MaterialProxy(new FExportMaterialProxy(InMaterial, InMaterialProperty));
	if (MaterialProxy == NULL)
	{
		return false;
	}

	FBox2D DummyBounds(FVector2D(0, 0), FVector2D(1, 1));
	TArray<FVector2D> EmptyTexCoords;
	FMaterialMergeData MaterialData(InMaterial, nullptr, nullptr, 0, DummyBounds, EmptyTexCoords);
	const bool bForceGamma = (InMaterialProperty == MP_Normal) || (InMaterialProperty == MP_OpacityMask) || (InMaterialProperty == MP_Opacity);	

	FIntPoint MaxSize = MaterialProxy->FindMaxTextureSize(InMaterial);
	FIntPoint OutSize = MaxSize;
	return RenderMaterialPropertyToTexture(MaterialData, InMaterialProperty, bForceGamma, PF_B8G8R8A8, MaxSize, OutSize, OutBMP);
}

bool FMaterialUtilities::ExportMaterialProperty(UWorld* InWorld, UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, FIntPoint& OutSize, TArray<FColor>& OutBMP)
{
	TScopedPointer<FExportMaterialProxy> MaterialProxy(new FExportMaterialProxy(InMaterial, InMaterialProperty));
	if (MaterialProxy == NULL)
	{
		return false;
	}

	FBox2D DummyBounds(FVector2D(0, 0), FVector2D(1, 1));
	TArray<FVector2D> EmptyTexCoords;
	FMaterialMergeData MaterialData(InMaterial, nullptr, nullptr, 0, DummyBounds, EmptyTexCoords);
	const bool bForceGamma = (InMaterialProperty == MP_Normal) || (InMaterialProperty == MP_OpacityMask) || (InMaterialProperty == MP_Opacity);
	OutSize = MaterialProxy->FindMaxTextureSize(InMaterial);
	return RenderMaterialPropertyToTexture(MaterialData, InMaterialProperty, bForceGamma, PF_B8G8R8A8, OutSize, OutSize, OutBMP);
}

bool FMaterialUtilities::ExportMaterialProperty(UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, TArray<FColor>& OutBMP, FIntPoint& OutSize)
{
	TScopedPointer<FExportMaterialProxy> MaterialProxy(new FExportMaterialProxy(InMaterial, InMaterialProperty));
	if (MaterialProxy == NULL)
	{
		return false;
	}

	FBox2D DummyBounds(FVector2D(0, 0), FVector2D(1, 1));
	TArray<FVector2D> EmptyTexCoords;
	FMaterialMergeData MaterialData(InMaterial, nullptr, nullptr, 0, DummyBounds, EmptyTexCoords);
	const bool bForceGamma = (InMaterialProperty == MP_Normal) || (InMaterialProperty == MP_OpacityMask) || (InMaterialProperty == MP_Opacity);
	OutSize = MaterialProxy->FindMaxTextureSize(InMaterial);
	return RenderMaterialPropertyToTexture(MaterialData, InMaterialProperty, bForceGamma, PF_B8G8R8A8, OutSize, OutSize, OutBMP);
}

bool FMaterialUtilities::ExportMaterialProperty(UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, FIntPoint InSize, TArray<FColor>& OutBMP)
{
	TScopedPointer<FExportMaterialProxy> MaterialProxy(new FExportMaterialProxy(InMaterial, InMaterialProperty));
	if (MaterialProxy == NULL)
	{
		return false;
	}

	FBox2D DummyBounds(FVector2D(0, 0), FVector2D(1, 1));
	TArray<FVector2D> EmptyTexCoords;
	FMaterialMergeData MaterialData(InMaterial, nullptr, nullptr, 0, DummyBounds, EmptyTexCoords);
	const bool bForceGamma = (InMaterialProperty == MP_Normal) || (InMaterialProperty == MP_OpacityMask) || (InMaterialProperty == MP_Opacity);
	FIntPoint OutSize;
	return RenderMaterialPropertyToTexture(MaterialData, InMaterialProperty, bForceGamma, PF_B8G8R8A8, InSize, OutSize, OutBMP);
}

bool FMaterialUtilities::ExportMaterial(UWorld* InWorld, UMaterialInterface* InMaterial, FFlattenMaterial& OutFlattenMaterial)
{
	return ExportMaterial(InMaterial, OutFlattenMaterial);
}

bool FMaterialUtilities::ExportMaterial(UMaterialInterface* InMaterial, FFlattenMaterial& OutFlattenMaterial, struct FExportMaterialProxyCache* ProxyCache)
{
	FBox2D DummyBounds(FVector2D(0, 0), FVector2D(1, 1));
	TArray<FVector2D> EmptyTexCoords;

	FMaterialMergeData MaterialData(InMaterial, nullptr, nullptr, 0, DummyBounds, EmptyTexCoords);
	ExportMaterial(MaterialData, OutFlattenMaterial, ProxyCache);
	return true;
}

bool FMaterialUtilities::ExportMaterial(UMaterialInterface* InMaterial, const FRawMesh* InMesh, int32 InMaterialIndex, const FBox2D& InTexcoordBounds, const TArray<FVector2D>& InTexCoords, FFlattenMaterial& OutFlattenMaterial, struct FExportMaterialProxyCache* ProxyCache)
{
	FMaterialMergeData MaterialData(InMaterial, InMesh, nullptr, InMaterialIndex, InTexcoordBounds, InTexCoords);
	ExportMaterial(MaterialData, OutFlattenMaterial, ProxyCache);
	return true;
}

bool FMaterialUtilities::ExportLandscapeMaterial(ALandscapeProxy* InLandscape, const TSet<FPrimitiveComponentId>& HiddenPrimitives, FFlattenMaterial& OutFlattenMaterial)
{
	check(InLandscape);

	FIntRect LandscapeRect = InLandscape->GetBoundingRect();
	FVector MidPoint = FVector(LandscapeRect.Min, 0.f) + FVector(LandscapeRect.Size(), 0.f)*0.5f;
		
	FVector LandscapeCenter = InLandscape->GetTransform().TransformPosition(MidPoint);
	FVector LandscapeExtent = FVector(LandscapeRect.Size(), 0.f)*InLandscape->GetActorScale()*0.5f; 

	FVector ViewOrigin = LandscapeCenter;
	FMatrix ViewRotationMatrix = FInverseRotationMatrix(InLandscape->GetActorRotation());
	ViewRotationMatrix*= FMatrix(FPlane(1,	0,	0,	0),
							FPlane(0,	-1,	0,	0),
							FPlane(0,	0,	-1,	0),
							FPlane(0,	0,	0,	1));
				
	const float ZOffset = WORLD_MAX;
	FMatrix ProjectionMatrix =  FReversedZOrthoMatrix(
		LandscapeExtent.X,
		LandscapeExtent.Y,
		0.5f / ZOffset,
		ZOffset);

	FSceneInterface* Scene = InLandscape->GetWorld()->Scene;
						
	// Render diffuse texture using BufferVisualizationMode=BaseColor	
	if (OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Diffuse))
	{
		const FIntPoint& DiffuseSize = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse);
		static const FName BaseColorName("BaseColor");
		const float BaseColorGamma = 2.2f; // BaseColor to gamma space
		TArray<FColor>& DiffuseSamples = OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse);
		RenderSceneToTexture(Scene, BaseColorName, ViewOrigin, ViewRotationMatrix, ProjectionMatrix, HiddenPrimitives, 
			DiffuseSize, BaseColorGamma, DiffuseSamples);
	}

	// Render normal map using BufferVisualizationMode=WorldNormal
	// Final material should use world space instead of tangent space for normals
	if (OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Normal))
	{
		static const FName WorldNormalName("WorldNormal");
		const float NormalColorGamma = 1.0f; // Dump normal texture in linear space
		const FIntPoint& NormalSize = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal);
		TArray<FColor>& NormalSamples = OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal);
		RenderSceneToTexture(Scene, WorldNormalName, ViewOrigin, ViewRotationMatrix, ProjectionMatrix, HiddenPrimitives, 
			NormalSize, NormalColorGamma, NormalSamples);
	}

	// Render metallic map using BufferVisualizationMode=Metallic	
	if (OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Metallic))
	{
		static const FName MetallicName("Metallic");
		const float MetallicColorGamma = 1.0f; // Dump metallic texture in linear space
		const FIntPoint& MetallicSize = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic);
		TArray<FColor>& MetallicSamples = OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic);
		RenderSceneToTexture(Scene, MetallicName, ViewOrigin, ViewRotationMatrix, ProjectionMatrix, HiddenPrimitives, 
			MetallicSize, MetallicColorGamma, MetallicSamples);
	}

	// Render roughness map using BufferVisualizationMode=Roughness	
	if (OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Roughness))
	{
		static const FName RoughnessName("Roughness");
		const float RoughnessColorGamma = 2.2f; // Roughness material powers color by 2.2, transform it back to linear
		const FIntPoint& RoughnessSize = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness);
		TArray<FColor>& RoughnessSamples = OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness);
		RenderSceneToTexture(Scene, RoughnessName, ViewOrigin, ViewRotationMatrix, ProjectionMatrix, HiddenPrimitives, 
			RoughnessSize, RoughnessColorGamma, RoughnessSamples);
	}

	// Render specular map using BufferVisualizationMode=Specular	
	if (OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Specular))
	{
		static const FName SpecularName("Specular");
		const float SpecularColorGamma = 1.0f; // Dump specular texture in linear space
		const FIntPoint& SpecularSize = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular);
		TArray<FColor>& SpecularSamples = OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular);
		RenderSceneToTexture(Scene, SpecularName, ViewOrigin, ViewRotationMatrix, ProjectionMatrix, HiddenPrimitives, 
			SpecularSize, SpecularColorGamma, SpecularSamples);
	}
				
	OutFlattenMaterial.MaterialId = InLandscape->GetLandscapeGuid();
	return true;
}

UMaterial* FMaterialUtilities::CreateMaterial(const FFlattenMaterial& InFlattenMaterial, UPackage* InOuter, const FString& BaseName, EObjectFlags Flags, const struct FMaterialProxySettings& MaterialProxySettings, TArray<UObject*>& OutGeneratedAssets, const TextureGroup& InTextureGroup /*= TEXTUREGROUP_World*/)
{
	// Base name for a new assets
	// In case outer is null BaseName has to be long package name
	if (InOuter == nullptr && FPackageName::IsShortPackageName(BaseName))
	{
		UE_LOG(LogMaterialUtilities, Warning, TEXT("Invalid long package name: '%s'."), *BaseName);
		return nullptr;
	}

	const FString AssetBaseName = FPackageName::GetShortName(BaseName);
	const FString AssetBasePath = InOuter ? TEXT("") : FPackageName::GetLongPackagePath(BaseName) + TEXT("/");
				
	// Create material
	const FString MaterialAssetName = TEXT("M_") + AssetBaseName;
	UPackage* MaterialOuter = InOuter;
	if (MaterialOuter == NULL)
	{
		MaterialOuter = CreatePackage(NULL, *(AssetBasePath + MaterialAssetName));
		MaterialOuter->FullyLoad();
		MaterialOuter->Modify();
	}

	UMaterial* Material = NewObject<UMaterial>(MaterialOuter, FName(*MaterialAssetName), Flags);
	Material->TwoSided = false;
	Material->DitheredLODTransition = InFlattenMaterial.bDitheredLODTransition;
	Material->SetShadingModel(MSM_DefaultLit);
	OutGeneratedAssets.Add(Material);

	int32 MaterialNodeY = -150;
	int32 MaterialNodeStepY = 180;

	// BaseColor
	const TArray<FColor>& DiffuseSamples = InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse);
	if (DiffuseSamples.Num() > 1)
	{
		const FString AssetName = TEXT("T_") + AssetBaseName + TEXT("_D");
		const FString AssetLongName = AssetBasePath + AssetName;
		const bool bSRGB = true;
		UTexture2D* Texture = CreateTexture(InOuter, AssetLongName, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse), DiffuseSamples, TC_Default, InTextureGroup, Flags, bSRGB);
		OutGeneratedAssets.Add(Texture);
			
		auto BasecolorExpression = NewObject<UMaterialExpressionTextureSample>(Material);
		BasecolorExpression->Texture = Texture;
		BasecolorExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Color;
		BasecolorExpression->MaterialExpressionEditorX = -400;
		BasecolorExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(BasecolorExpression);
		Material->BaseColor.Expression = BasecolorExpression;

		MaterialNodeY+= MaterialNodeStepY;
	}
	else if (DiffuseSamples.Num() == 1)
	{
		// Set Roughness to constant
		FLinearColor BaseColor = FLinearColor(DiffuseSamples[0]);
		auto BaseColorExpression = NewObject<UMaterialExpressionConstant4Vector>(Material);
		BaseColorExpression->Constant = BaseColor;
		BaseColorExpression->MaterialExpressionEditorX = -400;
		BaseColorExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(BaseColorExpression);
		Material->BaseColor.Expression = BaseColorExpression;

		MaterialNodeY += MaterialNodeStepY;
	}


	// Whether or not a material property is baked down
	const bool bHasMetallic = InFlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Metallic) && !InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Metallic);
	const bool bHasRoughness = InFlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Roughness) && !InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Roughness);
	const bool bHasSpecular = InFlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Specular) && !InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Specular);

	// Number of material properties baked down to textures
	const int BakedMaterialPropertyCount = bHasMetallic + bHasRoughness + bHasSpecular;

	// Check for same texture sizes
	bool bSameTextureSize = true;	

	int32 SampleCount = 0;
	FIntPoint MergedSize(0,0);
	for (int32 PropertyIndex = 0; PropertyIndex < 3; ++PropertyIndex)
	{
		EFlattenMaterialProperties Property = (EFlattenMaterialProperties)(PropertyIndex + (int32)EFlattenMaterialProperties::Metallic);
		const bool HasProperty = InFlattenMaterial.DoesPropertyContainData(Property) && !InFlattenMaterial.IsPropertyConstant(Property);
		FIntPoint PropertySize = InFlattenMaterial.GetPropertySize(Property);
		SampleCount = (bHasMetallic && SampleCount == 0) ? InFlattenMaterial.GetPropertySamples(Property).Num() : SampleCount;
		MergedSize = (bHasMetallic && MergedSize.X == 0) ? PropertySize : MergedSize;
	}
	bSameTextureSize &= bHasMetallic ? (SampleCount == InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic).Num()) : true;
	bSameTextureSize &= bHasRoughness ? (SampleCount == InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness).Num()) : true;
	bSameTextureSize &= bHasSpecular ? (SampleCount == InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular).Num()) : true;

	// Merge values into one texture if more than one material property exists
	if (BakedMaterialPropertyCount > 1 && bSameTextureSize)
	{
		// Metallic = R, Roughness = G, Specular = B
		TArray<FColor> MergedSamples;
		MergedSamples.AddZeroed(SampleCount);

		// R G B masks
#if PLATFORM_LITTLE_ENDIAN
		const uint32 ColorMask[3] = { 0x0000FF00, 0x00FF0000, 0xFF000000 };
#else // PLATFORM_LITTLE_ENDIAN
		const uint32 ColorMask[3] = { 0x00FF0000, 0x0000FF00, 0x000000FF };
#endif
		for (int32 PropertyIndex = 0; PropertyIndex < 3; ++PropertyIndex)
		{
			EFlattenMaterialProperties Property = (EFlattenMaterialProperties)(PropertyIndex + (int32)EFlattenMaterialProperties::Metallic);
			const bool HasProperty = InFlattenMaterial.DoesPropertyContainData(Property) && !InFlattenMaterial.IsPropertyConstant(Property);
			
			if (HasProperty)
			{
				const TArray<FColor>& PropertySamples = InFlattenMaterial.GetPropertySamples(Property);
				// OR masked values (samples initialized to zero, so no random data)
				for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
				{
					MergedSamples[SampleIndex].DWColor() |= (PropertySamples[SampleIndex].DWColor() & ColorMask[PropertyIndex]);
				}
			}
		}

		const FString AssetName = TEXT("T_") + AssetBaseName + TEXT("_MRS");
		const bool bSRGB = true;
		UTexture2D* Texture = CreateTexture(InOuter, AssetBasePath + AssetName, MergedSize, MergedSamples, TC_Default, InTextureGroup, Flags, bSRGB);
		OutGeneratedAssets.Add(Texture);

		auto MergedExpression = NewObject<UMaterialExpressionTextureSample>(Material);
		MergedExpression->Texture = Texture;
		MergedExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Color;
		MergedExpression->MaterialExpressionEditorX = -400;
		MergedExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(MergedExpression);

		// Metallic
		if (bHasMetallic)
		{
			Material->Metallic.Expression = MergedExpression;
			Material->Metallic.Mask = Material->Metallic.Expression->GetOutputs()[0].Mask;
			Material->Metallic.MaskR = 1;
			Material->Metallic.MaskG = 0;
			Material->Metallic.MaskB = 0;
			Material->Metallic.MaskA = 0;
		}

		// Roughness
		if (bHasRoughness)
		{
			Material->Roughness.Expression = MergedExpression;
			Material->Roughness.Mask = Material->Roughness.Expression->GetOutputs()[0].Mask;
			Material->Roughness.MaskR = 0;
			Material->Roughness.MaskG = 1;
			Material->Roughness.MaskB = 0;
			Material->Roughness.MaskA = 0;
		}
		
		// Specular
		if (bHasSpecular)
		{
			Material->Specular.Expression = MergedExpression;
			Material->Specular.Mask = Material->Specular.Expression->GetOutputs()[0].Mask;
			Material->Specular.MaskR = 0;
			Material->Specular.MaskG = 0;
			Material->Specular.MaskB = 1;
			Material->Specular.MaskA = 0;
		}

		MaterialNodeY += MaterialNodeStepY;
	}
	else
	{
		// Metallic
		if (bHasMetallic && MaterialProxySettings.bMetallicMap)
		{
			const FString AssetName = TEXT("T_") + AssetBaseName + TEXT("_M");
			const bool bSRGB = true;
			UTexture2D* Texture = CreateTexture(InOuter, AssetBasePath + AssetName, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic), InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic), TC_Default, InTextureGroup, Flags, bSRGB);
			OutGeneratedAssets.Add(Texture);

			auto MetallicExpression = NewObject<UMaterialExpressionTextureSample>(Material);
			MetallicExpression->Texture = Texture;
			MetallicExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Color;
			MetallicExpression->MaterialExpressionEditorX = -400;
			MetallicExpression->MaterialExpressionEditorY = MaterialNodeY;
			Material->Expressions.Add(MetallicExpression);
			Material->Metallic.Expression = MetallicExpression;

			MaterialNodeY += MaterialNodeStepY;
		}

		// Specular
		if (bHasSpecular && MaterialProxySettings.bSpecularMap)
		{
			const FString AssetName = TEXT("T_") + AssetBaseName + TEXT("_S");
			const bool bSRGB = true;
			UTexture2D* Texture = CreateTexture(InOuter, AssetBasePath + AssetName, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular), InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular), TC_Default, InTextureGroup, Flags, bSRGB);
			OutGeneratedAssets.Add(Texture);

			auto SpecularExpression = NewObject<UMaterialExpressionTextureSample>(Material);
			SpecularExpression->Texture = Texture;
			SpecularExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Color;
			SpecularExpression->MaterialExpressionEditorX = -400;
			SpecularExpression->MaterialExpressionEditorY = MaterialNodeY;
			Material->Expressions.Add(SpecularExpression);
			Material->Specular.Expression = SpecularExpression;

			MaterialNodeY += MaterialNodeStepY;
		}

		// Roughness
		if (bHasRoughness && MaterialProxySettings.bRoughnessMap)
		{
			const FString AssetName = TEXT("T_") + AssetBaseName + TEXT("_R");
			const bool bSRGB = true;
			UTexture2D* Texture = CreateTexture(InOuter, AssetBasePath + AssetName, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness), InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness), TC_Default, InTextureGroup, Flags, bSRGB);
			OutGeneratedAssets.Add(Texture);

			auto RoughnessExpression = NewObject<UMaterialExpressionTextureSample>(Material);
			RoughnessExpression->Texture = Texture;
			RoughnessExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Color;
			RoughnessExpression->MaterialExpressionEditorX = -400;
			RoughnessExpression->MaterialExpressionEditorY = MaterialNodeY;
			Material->Expressions.Add(RoughnessExpression);
			Material->Roughness.Expression = RoughnessExpression;

			MaterialNodeY += MaterialNodeStepY;
		}
	}

	if (InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Metallic) || !MaterialProxySettings.bMetallicMap)
	{
		auto MetallicExpression = NewObject<UMaterialExpressionConstant>(Material);
		MetallicExpression->R = MaterialProxySettings.bMetallicMap ? FLinearColor(InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic)[0]).R : MaterialProxySettings.MetallicConstant;
		MetallicExpression->MaterialExpressionEditorX = -400;
		MetallicExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(MetallicExpression);
		Material->Metallic.Expression = MetallicExpression;

		MaterialNodeY += MaterialNodeStepY;
	}

	if (InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Specular ) || !MaterialProxySettings.bSpecularMap)
	{
		// Set Specular to constant
		auto SpecularExpression = NewObject<UMaterialExpressionConstant>(Material);
		SpecularExpression->R = MaterialProxySettings.bSpecularMap ? FLinearColor(InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular)[0]).R : MaterialProxySettings.SpecularConstant;
		SpecularExpression->MaterialExpressionEditorX = -400;
		SpecularExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(SpecularExpression);
		Material->Specular.Expression = SpecularExpression;

		MaterialNodeY += MaterialNodeStepY;
	}
	
	if (InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Roughness) || !MaterialProxySettings.bRoughnessMap)
	{
		// Set Roughness to constant
		auto RoughnessExpression = NewObject<UMaterialExpressionConstant>(Material);
		RoughnessExpression->R = MaterialProxySettings.bRoughnessMap ? FLinearColor(InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness)[0]).R : MaterialProxySettings.RoughnessConstant;
		RoughnessExpression->MaterialExpressionEditorX = -400;
		RoughnessExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(RoughnessExpression);
		Material->Roughness.Expression = RoughnessExpression;

		MaterialNodeY += MaterialNodeStepY;
	}

	// Normal
	if (InFlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Normal) && !InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Normal))
	{
		const FString AssetName = TEXT("T_") + AssetBaseName + TEXT("_N");
		const bool bSRGB = false;
		UTexture2D* Texture = CreateTexture(InOuter, AssetBasePath + AssetName, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal), InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal), TC_Normalmap, (InTextureGroup != TEXTUREGROUP_World) ? InTextureGroup : TEXTUREGROUP_WorldNormalMap, Flags, bSRGB);
		OutGeneratedAssets.Add(Texture);
			
		auto NormalExpression = NewObject<UMaterialExpressionTextureSample>(Material);
		NormalExpression->Texture = Texture;
		NormalExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Normal;
		NormalExpression->MaterialExpressionEditorX = -400;
		NormalExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(NormalExpression);
		Material->Normal.Expression = NormalExpression;

		MaterialNodeY+= MaterialNodeStepY;
	}

	if (InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Emissive))
	{
		// Set Emissive to constant
		FColor EmissiveColor = InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive)[0];
		// Don't have to deal with black emissive color
		if (EmissiveColor != FColor(0, 0, 0, 255))
		{
			auto EmissiveColorExpression = NewObject<UMaterialExpressionConstant4Vector>(Material);
			EmissiveColorExpression->Constant = EmissiveColor.ReinterpretAsLinear() * InFlattenMaterial.EmissiveScale;
			EmissiveColorExpression->MaterialExpressionEditorX = -400;
			EmissiveColorExpression->MaterialExpressionEditorY = MaterialNodeY;
			Material->Expressions.Add(EmissiveColorExpression);
			Material->EmissiveColor.Expression = EmissiveColorExpression;

			MaterialNodeY += MaterialNodeStepY;
		}
	}
	else if (InFlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Emissive) && !InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Emissive))
	{
		const FString AssetName = TEXT("T_") + AssetBaseName + TEXT("_E");
		const bool bSRGB = true;
		UTexture2D* Texture = CreateTexture(InOuter, AssetBasePath + AssetName, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive), InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive), TC_Default, InTextureGroup, Flags, bSRGB);
		OutGeneratedAssets.Add(Texture);

		//Assign emissive color to the material
		UMaterialExpressionTextureSample* EmissiveColorExpression = NewObject<UMaterialExpressionTextureSample>(Material);
		EmissiveColorExpression->Texture = Texture;
		EmissiveColorExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Color;
		EmissiveColorExpression->MaterialExpressionEditorX = -400;
		EmissiveColorExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(EmissiveColorExpression);

		UMaterialExpressionMultiply* EmissiveColorScale = NewObject<UMaterialExpressionMultiply>(Material);
		EmissiveColorScale->A.Expression = EmissiveColorExpression;
		EmissiveColorScale->ConstB = InFlattenMaterial.EmissiveScale;
		EmissiveColorScale->MaterialExpressionEditorX = -200;
		EmissiveColorScale->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(EmissiveColorScale);

		Material->EmissiveColor.Expression = EmissiveColorScale;
		MaterialNodeY += MaterialNodeStepY;
	}

	if (InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Opacity))
	{
		// Set Opacity to constant
		FLinearColor Opacity = FLinearColor(InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity)[0]);
		auto OpacityExpression = NewObject<UMaterialExpressionConstant>(Material);
		OpacityExpression->R = Opacity.R;
		OpacityExpression->MaterialExpressionEditorX = -400;
		OpacityExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(OpacityExpression);
		Material->Opacity.Expression = OpacityExpression;

		MaterialNodeY += MaterialNodeStepY;
	}
	else if (InFlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Opacity))
	{
		const FString AssetName = TEXT("T_") + AssetBaseName + TEXT("_O");
		const bool bSRGB = true;
		UTexture2D* Texture = CreateTexture(InOuter, AssetBasePath + AssetName, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity), InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity), TC_Default, InTextureGroup, Flags, bSRGB);
		OutGeneratedAssets.Add(Texture);

		//Assign opacity to the material
		UMaterialExpressionTextureSample* OpacityExpression = NewObject<UMaterialExpressionTextureSample>(Material);
		OpacityExpression->Texture = Texture;
		OpacityExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Color;
		OpacityExpression->MaterialExpressionEditorX = -400;
		OpacityExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(OpacityExpression);
		Material->Opacity.Expression = OpacityExpression;
		MaterialNodeY += MaterialNodeStepY;
	}

	if (InFlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::SubSurface))
	{
		// Set Emissive to constant
		FColor SubSurfaceColor = InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::SubSurface)[0];

		// Don't have to deal with black sub surface color
		if (SubSurfaceColor != FColor(0, 0, 0, 255))
		{
			auto SubSurfaceColorExpression = NewObject<UMaterialExpressionConstant4Vector>(Material);
			SubSurfaceColorExpression->Constant = (SubSurfaceColor.ReinterpretAsLinear());
			SubSurfaceColorExpression->MaterialExpressionEditorX = -400;
			SubSurfaceColorExpression->MaterialExpressionEditorY = MaterialNodeY;
			Material->Expressions.Add(SubSurfaceColorExpression);
			Material->SubsurfaceColor.Expression = SubSurfaceColorExpression;

			MaterialNodeY += MaterialNodeStepY;
		}

		Material->SetShadingModel(MSM_Subsurface);

	}
	else if (InFlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::SubSurface))
	{
		const FString AssetName = TEXT("T_") + AssetBaseName + TEXT("_SSC");
		const bool bSRGB = true;
		UTexture2D* Texture = CreateTexture(InOuter, AssetBasePath + AssetName, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::SubSurface), InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::SubSurface), TC_Default, InTextureGroup, Flags, bSRGB);
		OutGeneratedAssets.Add(Texture);

		//Assign emissive color to the material
		UMaterialExpressionTextureSample* SubSurfaceColorExpression = NewObject<UMaterialExpressionTextureSample>(Material);
		SubSurfaceColorExpression->Texture = Texture;
		SubSurfaceColorExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Color;
		SubSurfaceColorExpression->MaterialExpressionEditorX = -400;
		SubSurfaceColorExpression->MaterialExpressionEditorY = MaterialNodeY;
		Material->Expressions.Add(SubSurfaceColorExpression);

		Material->SubsurfaceColor.Expression = SubSurfaceColorExpression;
		MaterialNodeY += MaterialNodeStepY;

		Material->SetShadingModel(MSM_Subsurface);
	}

							
	Material->PostEditChange();
	return Material;
}

UMaterialInstanceConstant* FMaterialUtilities::CreateInstancedMaterial(UMaterial* BaseMaterial, UPackage* InOuter, const FString& BaseName, EObjectFlags Flags)
{
	// Base name for a new assets
	// In case outer is null BaseName has to be long package name
	if (InOuter == nullptr && FPackageName::IsShortPackageName(BaseName))
	{
		UE_LOG(LogMaterialUtilities, Warning, TEXT("Invalid long package name: '%s'."), *BaseName);
		return nullptr;
	}

	const FString AssetBaseName = FPackageName::GetShortName(BaseName);
	const FString AssetBasePath = InOuter ? TEXT("") : FPackageName::GetLongPackagePath(BaseName) + TEXT("/");

	// Create material
	const FString MaterialAssetName = TEXT("M_") + AssetBaseName;
	UPackage* MaterialOuter = InOuter;
	if (MaterialOuter == NULL)
	{		
		MaterialOuter = CreatePackage(NULL, *(AssetBasePath + MaterialAssetName));
		MaterialOuter->FullyLoad();
		MaterialOuter->Modify();
	}

	// We need to check for this due to the change in material object type, this causes a clash of path/type with old assets that were generated, so we delete the old (resident) UMaterial objects
	UObject* ExistingPackage = FindObject<UMaterial>(MaterialOuter, *MaterialAssetName);
	if (ExistingPackage && !ExistingPackage->IsA<UMaterialInstanceConstant>())
	{
#if WITH_EDITOR
		FAutomationEditorCommonUtils::NullReferencesToObject(ExistingPackage);		
#endif // WITH_EDITOR
		ExistingPackage->MarkPendingKill();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
	}

	UMaterialInstanceConstant* MaterialInstance = NewObject<UMaterialInstanceConstant>(MaterialOuter, FName(*MaterialAssetName), Flags);
	checkf(MaterialInstance, TEXT("Failed to create instanced material"));
	MaterialInstance->Parent = BaseMaterial;	
	
	return MaterialInstance;
}

UTexture2D* FMaterialUtilities::CreateTexture(UPackage* Outer, const FString& AssetLongName, FIntPoint Size, const TArray<FColor>& Samples, TextureCompressionSettings CompressionSettings, TextureGroup LODGroup, EObjectFlags Flags, bool bSRGB, const FGuid& SourceGuidHash)
{
	FCreateTexture2DParameters TexParams;
	TexParams.bUseAlpha = false;
	TexParams.CompressionSettings = CompressionSettings;
	TexParams.bDeferCompression = true;
	TexParams.bSRGB = bSRGB;
	TexParams.SourceGuidHash = SourceGuidHash;

	if (Outer == nullptr)
	{
		Outer = CreatePackage(NULL, *AssetLongName);
		Outer->FullyLoad();
		Outer->Modify();
	}

	UTexture2D* Texture = FImageUtils::CreateTexture2D(Size.X, Size.Y, Samples, Outer, FPackageName::GetShortName(AssetLongName), Flags, TexParams);
	Texture->LODGroup = LODGroup;
	Texture->PostEditChange();
			
	return Texture;
}

bool FMaterialUtilities::ExportBaseColor(ULandscapeComponent* LandscapeComponent, int32 TextureSize, TArray<FColor>& OutSamples)
{
	ALandscapeProxy* LandscapeProxy = LandscapeComponent->GetLandscapeProxy();

	FIntPoint ComponentOrigin = LandscapeComponent->GetSectionBase() - LandscapeProxy->LandscapeSectionOffset;
	FIntPoint ComponentSize(LandscapeComponent->ComponentSizeQuads, LandscapeComponent->ComponentSizeQuads);
	FVector MidPoint = FVector(ComponentOrigin, 0.f) + FVector(ComponentSize, 0.f)*0.5f;

	FVector LandscapeCenter = LandscapeProxy->GetTransform().TransformPosition(MidPoint);
	FVector LandscapeExtent = FVector(ComponentSize, 0.f)*LandscapeProxy->GetActorScale()*0.5f;

	FVector ViewOrigin = LandscapeCenter;
	FMatrix ViewRotationMatrix = FInverseRotationMatrix(LandscapeProxy->GetActorRotation());
	ViewRotationMatrix *= FMatrix(FPlane(1, 0, 0, 0),
		FPlane(0, -1, 0, 0),
		FPlane(0, 0, -1, 0),
		FPlane(0, 0, 0, 1));

	const float ZOffset = WORLD_MAX;
	FMatrix ProjectionMatrix = FReversedZOrthoMatrix(
		LandscapeExtent.X,
		LandscapeExtent.Y,
		0.5f / ZOffset,
		ZOffset);

	FSceneInterface* Scene = LandscapeProxy->GetWorld()->Scene;

	// Hide all but the component
	TSet<FPrimitiveComponentId> HiddenPrimitives;
	for (auto PrimitiveComponentId : Scene->GetScenePrimitiveComponentIds())
	{
		HiddenPrimitives.Add(PrimitiveComponentId);
	}
	HiddenPrimitives.Remove(LandscapeComponent->SceneProxy->GetPrimitiveComponentId());
				
	FIntPoint TargetSize(TextureSize, TextureSize);

	// Render diffuse texture using BufferVisualizationMode=BaseColor
	static const FName BaseColorName("BaseColor");
	const float BaseColorGamma = 2.2f;
	RenderSceneToTexture(Scene, BaseColorName, ViewOrigin, ViewRotationMatrix, ProjectionMatrix, HiddenPrimitives, TargetSize, BaseColorGamma, OutSamples);
	return true;
}

FFlattenMaterial FMaterialUtilities::CreateFlattenMaterialWithSettings(const FMaterialProxySettings& InMaterialLODSettings)
{
	// Create new material.
	FFlattenMaterial Material;

	// TODO REMOVE THIS FEATURE?
	FIntPoint MaximumSize = InMaterialLODSettings.TextureSize;	
	// If the user is manually overriding the texture size, make sure we have the max texture size to render with
	if (InMaterialLODSettings.TextureSizingType == TextureSizingType_UseManualOverrideTextureSize)
	{
		MaximumSize = (MaximumSize.X < InMaterialLODSettings.DiffuseTextureSize.X) ? MaximumSize : InMaterialLODSettings.DiffuseTextureSize;
		MaximumSize = (InMaterialLODSettings.bSpecularMap && MaximumSize.X < InMaterialLODSettings.SpecularTextureSize.X) ? MaximumSize : InMaterialLODSettings.SpecularTextureSize;
		MaximumSize = (InMaterialLODSettings.bMetallicMap && MaximumSize.X < InMaterialLODSettings.MetallicTextureSize.X) ? MaximumSize : InMaterialLODSettings.MetallicTextureSize;
		MaximumSize = (InMaterialLODSettings.bRoughnessMap && MaximumSize.X < InMaterialLODSettings.RoughnessTextureSize.X) ? MaximumSize : InMaterialLODSettings.RoughnessTextureSize;
		MaximumSize = (InMaterialLODSettings.bNormalMap && MaximumSize.X < InMaterialLODSettings.NormalTextureSize.X) ? MaximumSize : InMaterialLODSettings.NormalTextureSize;
		MaximumSize = (InMaterialLODSettings.bEmissiveMap && MaximumSize.X < InMaterialLODSettings.EmissiveTextureSize.X) ? MaximumSize : InMaterialLODSettings.EmissiveTextureSize;
		MaximumSize = (InMaterialLODSettings.bOpacityMap && MaximumSize.X < InMaterialLODSettings.OpacityTextureSize.X) ? MaximumSize : InMaterialLODSettings.OpacityTextureSize;
	}
	
	if (InMaterialLODSettings.TextureSizingType == TextureSizingType_UseManualOverrideTextureSize)
	{
		Material.RenderSize = MaximumSize;

		Material.SetPropertySize(EFlattenMaterialProperties::Diffuse, InMaterialLODSettings.DiffuseTextureSize);
		Material.SetPropertySize(EFlattenMaterialProperties::Specular, InMaterialLODSettings.bSpecularMap ? InMaterialLODSettings.SpecularTextureSize : FIntPoint::ZeroValue);
		Material.SetPropertySize(EFlattenMaterialProperties::Metallic, InMaterialLODSettings.bMetallicMap ? InMaterialLODSettings.MetallicTextureSize : FIntPoint::ZeroValue);
		Material.SetPropertySize(EFlattenMaterialProperties::Roughness, InMaterialLODSettings.bRoughnessMap ? InMaterialLODSettings.RoughnessTextureSize : FIntPoint::ZeroValue);
		Material.SetPropertySize(EFlattenMaterialProperties::Normal, InMaterialLODSettings.bNormalMap ? InMaterialLODSettings.NormalTextureSize : FIntPoint::ZeroValue);
		Material.SetPropertySize(EFlattenMaterialProperties::Emissive, InMaterialLODSettings.bEmissiveMap ? InMaterialLODSettings.EmissiveTextureSize : FIntPoint::ZeroValue);
		Material.SetPropertySize(EFlattenMaterialProperties::Opacity, InMaterialLODSettings.bOpacityMap ? InMaterialLODSettings.OpacityTextureSize : FIntPoint::ZeroValue);
	}
	else if (InMaterialLODSettings.TextureSizingType == TextureSizingType_UseAutomaticBiasedSizes)
	{
		Material.RenderSize = InMaterialLODSettings.TextureSize;
		
		int NormalSizeX, DiffuseSizeX, PropertiesSizeX;
		NormalSizeX = InMaterialLODSettings.TextureSize.X;
		DiffuseSizeX = FMath::Max(InMaterialLODSettings.TextureSize.X >> 1, 32);
		PropertiesSizeX = FMath::Max(InMaterialLODSettings.TextureSize.X >> 2, 16);

		Material.SetPropertySize(EFlattenMaterialProperties::Diffuse, FIntPoint(DiffuseSizeX, DiffuseSizeX));
		Material.SetPropertySize(EFlattenMaterialProperties::Normal, (InMaterialLODSettings.bNormalMap) ? FIntPoint(NormalSizeX, NormalSizeX) : FIntPoint::ZeroValue);

		FIntPoint PropertiesSize = FIntPoint(PropertiesSizeX, PropertiesSizeX);
		Material.SetPropertySize(EFlattenMaterialProperties::Specular, (InMaterialLODSettings.bSpecularMap) ? PropertiesSize : FIntPoint::ZeroValue );
		Material.SetPropertySize(EFlattenMaterialProperties::Metallic, (InMaterialLODSettings.bMetallicMap) ? PropertiesSize : FIntPoint::ZeroValue );
		Material.SetPropertySize(EFlattenMaterialProperties::Roughness, (InMaterialLODSettings.bRoughnessMap) ? PropertiesSize : FIntPoint::ZeroValue );
		Material.SetPropertySize(EFlattenMaterialProperties::Emissive, (InMaterialLODSettings.bEmissiveMap) ? PropertiesSize : FIntPoint::ZeroValue );
		Material.SetPropertySize(EFlattenMaterialProperties::Opacity, (InMaterialLODSettings.bOpacityMap) ? PropertiesSize : FIntPoint::ZeroValue );
	}
	
	Material.RenderSize = InMaterialLODSettings.TextureSize;	
	Material.SetPropertySize(EFlattenMaterialProperties::Diffuse , InMaterialLODSettings.TextureSize);
	Material.SetPropertySize(EFlattenMaterialProperties::Specular, (InMaterialLODSettings.bSpecularMap) ? InMaterialLODSettings.TextureSize : FIntPoint::ZeroValue);
	Material.SetPropertySize(EFlattenMaterialProperties::Metallic, (InMaterialLODSettings.bMetallicMap) ? InMaterialLODSettings.TextureSize : FIntPoint::ZeroValue);
	Material.SetPropertySize(EFlattenMaterialProperties::Roughness,  (InMaterialLODSettings.bRoughnessMap) ? InMaterialLODSettings.TextureSize : FIntPoint::ZeroValue);
	Material.SetPropertySize(EFlattenMaterialProperties::Normal,  (InMaterialLODSettings.bNormalMap) ? InMaterialLODSettings.TextureSize : FIntPoint::ZeroValue);
	Material.SetPropertySize(EFlattenMaterialProperties::Emissive,  (InMaterialLODSettings.bEmissiveMap) ? InMaterialLODSettings.TextureSize : FIntPoint::ZeroValue);
	Material.SetPropertySize(EFlattenMaterialProperties::Opacity,  (InMaterialLODSettings.bOpacityMap) ? InMaterialLODSettings.TextureSize : FIntPoint::ZeroValue);

	return Material;
}

void FMaterialUtilities::AnalyzeMaterial(UMaterialInterface* InMaterial, const struct FMaterialProxySettings& InMaterialSettings, int32& OutNumTexCoords, bool& OutRequiresVertexData)
{
	OutRequiresVertexData = false;
	OutNumTexCoords = 0;

	bool PropertyBeingBaked[MP_Normal + 1];	
	PropertyBeingBaked[MP_BaseColor] = true;
	PropertyBeingBaked[MP_Specular] = InMaterialSettings.bSpecularMap;
	PropertyBeingBaked[MP_Roughness] = InMaterialSettings.bRoughnessMap;
	PropertyBeingBaked[MP_Metallic] = InMaterialSettings.bMetallicMap;
	PropertyBeingBaked[MP_Normal] = InMaterialSettings.bNormalMap;
	PropertyBeingBaked[MP_Metallic] = InMaterialSettings.bOpacityMap;
	PropertyBeingBaked[MP_EmissiveColor] = InMaterialSettings.bEmissiveMap;

	for (int32 PropertyIndex = 0; PropertyIndex < ARRAY_COUNT(PropertyBeingBaked); ++PropertyIndex)
	{
		if (PropertyBeingBaked[PropertyIndex])
		{
			EMaterialProperty Property = (EMaterialProperty)PropertyIndex;

			if (PropertyIndex == MP_Opacity)
			{
				EBlendMode BlendMode = InMaterial->GetBlendMode();
				if (BlendMode == BLEND_Masked)
				{
					Property = MP_OpacityMask;
				}
				else if (IsTranslucentBlendMode(BlendMode))
				{
					Property = MP_Opacity;
				}
				else
				{
					continue;
				}
			}

			// Analyze this material channel.
			int32 NumTextureCoordinates = 0;
			bool bUseVertexData = false;
			InMaterial->AnalyzeMaterialProperty(Property, NumTextureCoordinates, bUseVertexData);
			// Accumulate data.
			OutNumTexCoords = FMath::Max(NumTextureCoordinates, OutNumTexCoords);
			OutRequiresVertexData |= bUseVertexData;
		}
	}
}

void FMaterialUtilities::RemapUniqueMaterialIndices(const TArray<UMaterialInterface*>& InMaterials, const TArray<FRawMeshExt>& InMeshData, const TMap<FMeshIdAndLOD, TArray<int32> >& InMaterialMap, const FMaterialProxySettings& InMaterialProxySettings, const bool bBakeVertexData, const bool bMergeMaterials, TArray<bool>& OutMeshShouldBakeVertexData, TMap<FMeshIdAndLOD, TArray<int32> >& OutMaterialMap, TArray<UMaterialInterface*>& OutMaterials)
{
	// Gather material properties
	TMap<UMaterialInterface*, int32> MaterialNumTexCoords;
	TMap<UMaterialInterface*, bool>  MaterialUseVertexData;

	for (int32 MaterialIndex = 0; MaterialIndex < InMaterials.Num(); MaterialIndex++)
	{
		UMaterialInterface* Material = InMaterials[MaterialIndex];
		if (MaterialNumTexCoords.Find(Material) != nullptr)
		{
			// This material was already processed.
			continue;
		}

		if (!bBakeVertexData || !bMergeMaterials)
		{
			// We are not baking vertex data at all, don't analyze materials.
			MaterialNumTexCoords.Add(Material, 0);
			MaterialUseVertexData.Add(Material, false);
			continue;
		}
		int32 NumTexCoords = 0;
		bool bUseVertexData = false;
		FMaterialUtilities::AnalyzeMaterial(Material, InMaterialProxySettings, NumTexCoords, bUseVertexData);
		MaterialNumTexCoords.Add(Material, NumTexCoords);
		MaterialUseVertexData.Add(Material, bUseVertexData);
	}

	// Build list of mesh's material properties.
	OutMeshShouldBakeVertexData.Empty();
	OutMeshShouldBakeVertexData.AddZeroed(InMeshData.Num());

	TArray<bool> MeshUsesVertexData;
	MeshUsesVertexData.AddZeroed(InMeshData.Num());

	for (int32 MeshIndex = 0; MeshIndex < InMeshData.Num(); MeshIndex++)
	{
		for (int32 LODIndex = 0; LODIndex < MAX_STATIC_MESH_LODS; ++LODIndex)
		{
			if (InMeshData[MeshIndex].bShouldExportLOD[LODIndex])
			{
				checkf(InMeshData[MeshIndex].MeshLODData[LODIndex].RawMesh->VertexPositions.Num(), TEXT("No vertex data found in mesh LOD"));
				const TArray<int32>& MeshMaterialMap = InMaterialMap[FMeshIdAndLOD(MeshIndex, LODIndex)];
				int32 NumTexCoords = 0;
				bool bUseVertexData = false;
				// Accumulate data of all materials.
				for (int32 LocalMaterialIndex = 0; LocalMaterialIndex < MeshMaterialMap.Num(); LocalMaterialIndex++)
				{
					UMaterialInterface* Material = InMaterials[MeshMaterialMap[LocalMaterialIndex]];
					NumTexCoords = FMath::Max(NumTexCoords, MaterialNumTexCoords[Material]);
					bUseVertexData |= MaterialUseVertexData[Material];
				}
				// Store data.
				MeshUsesVertexData[MeshIndex] = bUseVertexData;
				OutMeshShouldBakeVertexData[MeshIndex] = bUseVertexData || (NumTexCoords >= 2);
			}
		}
	}

	// Build new material map.
	// Structure used to simplify material merging.
	struct FMeshMaterialData
	{
		UMaterialInterface* Material;
		UStaticMesh* Mesh;
		bool bHasVertexColors;

		FMeshMaterialData(UMaterialInterface* InMaterial, UStaticMesh* InMesh, bool bInHasVertexColors)
			: Material(InMaterial)
			, Mesh(InMesh)
			, bHasVertexColors(bInHasVertexColors)
		{
		}

		bool operator==(const FMeshMaterialData& Other) const
		{
			return Material == Other.Material && Mesh == Other.Mesh && bHasVertexColors == Other.bHasVertexColors;
		}
	};

	TArray<FMeshMaterialData> MeshMaterialData;
	OutMaterialMap.Empty();
	for (int32 MeshIndex = 0; MeshIndex < InMeshData.Num(); MeshIndex++)
	{
		for (int32 LODIndex = 0; LODIndex < MAX_STATIC_MESH_LODS; ++LODIndex)
		{
			if (InMeshData[MeshIndex].bShouldExportLOD[LODIndex])
			{
				checkf(InMeshData[MeshIndex].MeshLODData[LODIndex].RawMesh->VertexPositions.Num(), TEXT("No vertex data found in mesh LOD"));

				const TArray<int32>& MeshMaterialMap = InMaterialMap[FMeshIdAndLOD(MeshIndex, LODIndex)];
				TArray<int32>& NewMeshMaterialMap = OutMaterialMap.Add(FMeshIdAndLOD(MeshIndex, LODIndex));
				UStaticMesh* StaticMesh = InMeshData[MeshIndex].SourceStaticMesh;

				if (!MeshUsesVertexData[MeshIndex])
				{
					// No vertex data needed - could merge materials with other meshes.
					if (!OutMeshShouldBakeVertexData[MeshIndex])
					{
						// Set to 'nullptr' if don't need to bake vertex data to be able to merge materials with any meshes
						// which don't require vertex data baking too.
						StaticMesh = nullptr;
					}

					for (int32 LocalMaterialIndex = 0; LocalMaterialIndex < MeshMaterialMap.Num(); LocalMaterialIndex++)
					{
						FMeshMaterialData Data(InMaterials[MeshMaterialMap[LocalMaterialIndex]], StaticMesh, false);
						int32 Index = MeshMaterialData.Find(Data);
						if (Index == INDEX_NONE)
						{
							// Not found, add new entry.
							Index = MeshMaterialData.Add(Data);
						}
						NewMeshMaterialMap.Add(Index);
					}
				}
				else
				{
					// Mesh with vertex data baking, and with vertex colors - don't share materials at all.
					for (int32 LocalMaterialIndex = 0; LocalMaterialIndex < MeshMaterialMap.Num(); LocalMaterialIndex++)
					{
						FMeshMaterialData Data(InMaterials[MeshMaterialMap[LocalMaterialIndex]], StaticMesh, true);
						int32 Index = MeshMaterialData.Add(Data);
						NewMeshMaterialMap.Add(Index);
					}
				}
			}
		}
	}

	// Build new material list - simply extract MeshMaterialData[i].Material.
	OutMaterials.Empty();
	OutMaterials.AddUninitialized(MeshMaterialData.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < MeshMaterialData.Num(); MaterialIndex++)
	{
		OutMaterials[MaterialIndex] = MeshMaterialData[MaterialIndex].Material;
	}
}

void FMaterialUtilities::OptimizeFlattenMaterial(FFlattenMaterial& InFlattenMaterial)
{
	// Try to optimize each individual property sample
	for (int32 PropertyIndex = 0; PropertyIndex < (int32)EFlattenMaterialProperties::NumFlattenMaterialProperties; ++PropertyIndex)
	{
		EFlattenMaterialProperties Property = (EFlattenMaterialProperties)PropertyIndex;
		FIntPoint Size = InFlattenMaterial.GetPropertySize(Property);
		OptimizeSampleArray(InFlattenMaterial.GetPropertySamples(Property), Size);
		InFlattenMaterial.SetPropertySize(Property, Size);
	}
}

void FMaterialUtilities::ResizeFlattenMaterial(FFlattenMaterial& InFlattenMaterial, const struct FMeshProxySettings& InMeshProxySettings)
{
	const FMaterialProxySettings& MaterialSettings = InMeshProxySettings.MaterialSettings;
	if (MaterialSettings.TextureSizingType == TextureSizingType_UseAutomaticBiasedSizes)
	{
		int NormalSizeX, DiffuseSizeX, PropertiesSizeX;
		NormalSizeX = MaterialSettings.TextureSize.X;
		DiffuseSizeX = FMath::Max(MaterialSettings.TextureSize.X >> 1, 32);
		PropertiesSizeX = FMath::Max(MaterialSettings.TextureSize.X >> 2, 16);

		if (InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse).X != DiffuseSizeX)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse), DiffuseSizeX, DiffuseSizeX, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Diffuse, FIntPoint(DiffuseSizeX, DiffuseSizeX));
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular).Num() && MaterialSettings.bSpecularMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular).X != PropertiesSizeX)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular), PropertiesSizeX, PropertiesSizeX, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Specular, FIntPoint(PropertiesSizeX, PropertiesSizeX));
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic).Num() && MaterialSettings.bMetallicMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic).X != PropertiesSizeX)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic), PropertiesSizeX, PropertiesSizeX, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Metallic, FIntPoint(PropertiesSizeX, PropertiesSizeX));
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness).Num() && MaterialSettings.bRoughnessMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness).X != PropertiesSizeX)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness), PropertiesSizeX, PropertiesSizeX, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Roughness, FIntPoint(PropertiesSizeX, PropertiesSizeX));
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal).Num() && MaterialSettings.bNormalMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal).X != NormalSizeX)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal), NormalSizeX, NormalSizeX, NewSamples, true);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Normal, FIntPoint(NormalSizeX, NormalSizeX));
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive).Num() && MaterialSettings.bEmissiveMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive).X != PropertiesSizeX)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive), PropertiesSizeX, PropertiesSizeX, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Emissive, FIntPoint(PropertiesSizeX, PropertiesSizeX));
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity).Num() && MaterialSettings.bOpacityMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity).X != PropertiesSizeX)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity), PropertiesSizeX, PropertiesSizeX, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Opacity, FIntPoint(PropertiesSizeX, PropertiesSizeX));
		}
	}
	else if (MaterialSettings.TextureSizingType == TextureSizingType_UseManualOverrideTextureSize)
	{
		if (InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse) != MaterialSettings.DiffuseTextureSize)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse), MaterialSettings.DiffuseTextureSize.X, MaterialSettings.DiffuseTextureSize.Y, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Diffuse, MaterialSettings.DiffuseTextureSize);
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular).Num() && MaterialSettings.bSpecularMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular) != MaterialSettings.SpecularTextureSize)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular), MaterialSettings.SpecularTextureSize.X, MaterialSettings.SpecularTextureSize.Y, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Specular, MaterialSettings.SpecularTextureSize);
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic).Num() && MaterialSettings.bMetallicMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic) != MaterialSettings.MetallicTextureSize)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic), MaterialSettings.MetallicTextureSize.X, MaterialSettings.MetallicTextureSize.Y, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Metallic, MaterialSettings.MetallicTextureSize);
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness).Num() && MaterialSettings.bRoughnessMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness) != MaterialSettings.RoughnessTextureSize)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness), MaterialSettings.RoughnessTextureSize.X, MaterialSettings.RoughnessTextureSize.Y, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Roughness, MaterialSettings.RoughnessTextureSize);
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal).Num() && MaterialSettings.bNormalMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal) != MaterialSettings.NormalTextureSize)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal), MaterialSettings.NormalTextureSize.X, MaterialSettings.NormalTextureSize.Y, NewSamples, true);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Normal, MaterialSettings.NormalTextureSize);
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive).Num() && MaterialSettings.bEmissiveMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive) != MaterialSettings.EmissiveTextureSize)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive), MaterialSettings.EmissiveTextureSize.X, MaterialSettings.EmissiveTextureSize.Y, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Emissive, MaterialSettings.EmissiveTextureSize);
		}

		if (InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity).Num() && MaterialSettings.bOpacityMap && InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity) != MaterialSettings.OpacityTextureSize)
		{
			TArray<FColor> NewSamples;
			FImageUtils::ImageResize(InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity).X, InFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity).Y, InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity), MaterialSettings.OpacityTextureSize.X, MaterialSettings.OpacityTextureSize.Y, NewSamples, false);
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity).Reset(NewSamples.Num());
			InFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity).Append(NewSamples);
			InFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Opacity, MaterialSettings.OpacityTextureSize);
		}
	}
}

/** Computes the uniform scale from the input scales,  if one exists. */
static bool GetUniformScale(const TArray<float> Scales, float& UniformScale)
{
	if (Scales.Num())
	{
		float Average = 0;
		float Mean = 0;

		for (float V : Scales)
		{
			Average += V;
		}
		Average /= (float)Scales.Num();

		for (float V : Scales)
		{
			Mean += FMath::Abs(V - Average);
		}
		Mean /= (float)Scales.Num();

		if (Mean * 15.f < Average) // If they are almost all the same
		{
			UniformScale = Average;
			return true;
		}
		else // Otherwise do a much more expensive test by counting the number of similar values
		{
			// Try to find a small range where 80% of values fit within.
			const int32 TryThreshold = FMath::CeilToInt(.80f * (float)Scales.Num());

			int32 NextTryDomain = Scales.Num();

			float NextTryMinV = 1024;
			for (float V : Scales)
			{
				NextTryMinV = FMath::Min<float>(V, NextTryMinV);
			}

			while (NextTryDomain >= TryThreshold) // Stop the search it is garantied to fail.
			{
				float TryMinV = NextTryMinV;
				float TryMaxV = TryMinV * 1.25f;
				int32 TryMatches = 0;
				NextTryMinV = 1024;
				NextTryDomain = 0;
				for (float V : Scales)
				{
					if (TryMinV <= V && V <= TryMaxV)
					{
						++TryMatches;
					}

					if (V > TryMinV)
					{
						NextTryMinV = FMath::Min<float>(V, NextTryMinV);
						++NextTryDomain;
					}
				}

				if (TryMatches >= TryThreshold)
				{
					UniformScale = TryMinV;
					return true;
				}
			}
		}
	}
	return false;
}

uint32 GetTypeHash(const FMaterialUtilities::FExportErrorManager::FError& Error)
{
	return GetTypeHash(Error.Material);
}

bool FMaterialUtilities::FExportErrorManager::FError::operator==(const FError& Rhs) const
{
	return Material == Rhs.Material && RegisterIndex == Rhs.RegisterIndex && ErrorType == Rhs.ErrorType;
}


void FMaterialUtilities::FExportErrorManager::Register(const UMaterialInterface* Material, const UTexture* Texture, int32 RegisterIndex, EErrorType ErrorType)
{
	if (!Material || !Texture) return;

	FError Error;
	Error.Material = Material->GetMaterialResource(FeatureLevel);
	if (!Error.Material) return;
	Error.RegisterIndex = RegisterIndex;
	Error.ErrorType = ErrorType;

	FInstance Instance;
	Instance.Material = Material;
	Instance.Texture = Texture;

	ErrorInstances.FindOrAdd(Error).Push(Instance);
}

void FMaterialUtilities::FExportErrorManager::OutputToLog()
{
	const UMaterialInterface* CurrentMaterial = nullptr;
	int32 MaxInstanceCount = 0;
	FString TextureErrors;

	for (TMap<FError, TArray<FInstance> >::TIterator It(ErrorInstances);; ++It)
	{
		if (It && !It->Value.Num()) continue;

		// Here we pack texture list per material.
		if (!It || CurrentMaterial != It->Value[0].Material)
		{
			// Flush
			if (CurrentMaterial)
			{
				FString SimilarCount(TEXT(""));
				if (MaxInstanceCount > 1)
				{
					SimilarCount = FString::Printf(TEXT(", %d similar"), MaxInstanceCount - 1);
				}

				if (CurrentMaterial == CurrentMaterial->GetMaterial())
				{
					UE_LOG(TextureStreamingBuild, Verbose, TEXT("Incomplete texcoord scale analysis for %s%s: %s"), *CurrentMaterial->GetName(), *SimilarCount, *TextureErrors);
				}
				else
				{
					UE_LOG(TextureStreamingBuild, Verbose, TEXT("Incomplete texcoord scale analysis for %s, UMaterial=%s%s: %s"), *CurrentMaterial->GetName(), *CurrentMaterial->GetMaterial()->GetName(), *SimilarCount, *TextureErrors);
				}
			}

			// Exit
			if (!It)
			{
				break;
			}

			// Start new
			CurrentMaterial = It->Value[0].Material;
			MaxInstanceCount = It->Value.Num();
			TextureErrors.Empty();
		}
		else
		{
			// Append
			MaxInstanceCount = FMath::Max<int32>(MaxInstanceCount, It->Value.Num());
		}

		const TCHAR* ErrorMsg = TEXT("Unkown Error");
		if (It->Key.ErrorType == EET_IncohorentValues)
		{
			ErrorMsg = TEXT("Incoherent");
		}
		else if (It->Key.ErrorType == EET_NoValues)
		{
			ErrorMsg = TEXT("NoValues");
		}

		TextureErrors.Append(FString::Printf(TEXT("(%s:%d,%s) "), ErrorMsg, It->Key.RegisterIndex, *It->Value[0].Texture->GetName()));
	}
}

bool FMaterialUtilities::ExportMaterialTexCoordScales(UMaterialInterface* InMaterial, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TArray<FMaterialTexCoordBuildInfo>& OutScales, FExportErrorManager& OutErrors)
{
	TArray<FFloat16Color> RenderedVectors;

	TArray<UTexture*> Textures;
	TArray< TArray<int32> > Indices;
	InMaterial->GetUsedTexturesAndIndices(Textures, Indices, QualityLevel, FeatureLevel);

	check(Textures.Num() >= Indices.Num()); // Can't have indices if no texture.

	// Clear any entry not related to UTexture2D
	for (int32 Index = 0; Index < Textures.Num(); ++Index)
	{
		if (!Cast<UTexture2D>(Textures[Index]))
		{
			Indices[Index].Empty(); 
		}
	}

	const int32 SCALE_PRECISION = 64.f;
	const int32 TILE_RESOLUTION = FMaterialTexCoordBuildInfo::TILE_RESOLUTION;
	const int32 INITIAL_GPU_SCALE = FMaterialTexCoordBuildInfo::INITIAL_GPU_SCALE;
	const int32 MAX_NUM_TEX_COORD = FMaterialTexCoordBuildInfo::MAX_NUM_TEX_COORD;
	const int32 MAX_NUM_TEXTURE_REGISTER = FMaterialTexCoordBuildInfo::MAX_NUM_TEXTURE_REGISTER;

	const bool bUseMetrics = CVarStreamingUseNewMetrics.GetValueOnGameThread() != 0;

	int32 MaxRegisterIndex = INDEX_NONE;

	for (const TArray<int32>& TextureIndices : Indices)
	{
		for (int32 RegisterIndex : TextureIndices)
		{
			MaxRegisterIndex = FMath::Max<int32>(RegisterIndex, MaxRegisterIndex);
		}
	}

	if (MaxRegisterIndex == INDEX_NONE)
	{
		return false;
	}

	TBitArray<> RegisterInUse(false, MaxRegisterIndex + 1);

	// Set the validity flag.
	for (const TArray<int32>& TextureIndices : Indices)
	{
		for (int32 RegisterIndex : TextureIndices)
		{
			RegisterInUse[RegisterIndex] = true;
		}
	}


	const int32 NumTileX = (MaxRegisterIndex / 4 + 1);
	const int32 NumTileY = MAX_NUM_TEX_COORD;
	FIntPoint RenderTargetSize(TILE_RESOLUTION * NumTileX, TILE_RESOLUTION * NumTileY);

	// Render the vectors
	{
		// The rendertarget contain factors stored in XYZW. Every X tile maps to another group : (0, 1, 2, 3), (4, 5, 6, 7), ...
		UTextureRenderTarget2D* RenderTarget = CreateRenderTarget(true, false, PF_FloatRGBA, RenderTargetSize);

		// Allocate the render output.
		RenderedVectors.Empty(RenderTargetSize.X * RenderTargetSize.Y);

		FMaterialRenderProxy* MaterialProxy = InMaterial->GetRenderProxy(false, false);
		if (!MaterialProxy)
		{
			return false;
		}

		FBox2D DummyBounds(FVector2D(0, 0), FVector2D(1, 1));
		TArray<FVector2D> EmptyTexCoords;
		FMaterialMergeData MaterialData(InMaterial, nullptr, nullptr, 0, DummyBounds, EmptyTexCoords);

		CurrentlyRendering = true;
		bool bResult = FMeshRenderer::RenderMaterialTexCoordScales(MaterialData, MaterialProxy, RenderTarget, RenderedVectors);
		CurrentlyRendering = false;

		if (!bResult)
		{
			return false;
		}
	}

	OutScales.AddDefaulted(MaxRegisterIndex + 1);

	TArray<float> TextureIndexScales;

	// Now compute the scale for each
	for (int32 RegisterIndex = 0; RegisterIndex <= MaxRegisterIndex; ++RegisterIndex)
	{
		if (!RegisterInUse[RegisterIndex]) continue;

		// If nothing works, this will fallback to 1.f
		OutScales[RegisterIndex].Scale = 1.f;
		OutScales[RegisterIndex].Index = 0;

		if (!bUseMetrics) continue; // Old metrics would always use 1.f

		int32 TextureTile = RegisterIndex / 4;
		int32 ComponentIndex = RegisterIndex % 4;

		bool bSuccess = false;
		for (int32 CoordIndex = 0; CoordIndex < MAX_NUM_TEX_COORD && !bSuccess; ++CoordIndex)
		{
			TextureIndexScales.Empty(TILE_RESOLUTION * TILE_RESOLUTION);
			for (int32 TexelX = 0; TexelX < TILE_RESOLUTION; ++TexelX)
			{
				for (int32 TexelY = 0; TexelY < TILE_RESOLUTION; ++TexelY)
				{
					int32 TexelIndex = TextureTile * TILE_RESOLUTION + TexelX + (TexelY + CoordIndex * TILE_RESOLUTION) * RenderTargetSize.X;
					FFloat16Color& Scale16 = RenderedVectors[TexelIndex];

					float TexelScale = 0;
					if (ComponentIndex == 0) TexelScale = Scale16.R.GetFloat();
					if (ComponentIndex == 1) TexelScale = Scale16.G.GetFloat();
					if (ComponentIndex == 2) TexelScale = Scale16.B.GetFloat();
					if (ComponentIndex == 3) TexelScale = Scale16.A.GetFloat();

					// Quantize scale to converge faster in the TryLogic
					TexelScale = FMath::RoundToFloat(TexelScale * SCALE_PRECISION) / SCALE_PRECISION;

					if (TexelScale > 0 && TexelScale < INITIAL_GPU_SCALE)
					{
						TextureIndexScales.Push(TexelScale);
					}
				}
			}

			if (GetUniformScale(TextureIndexScales, OutScales[RegisterIndex].Scale))
			{
				OutScales[RegisterIndex].Index = CoordIndex;
				bSuccess = true;
			}
		}

		// If we couldn't find the scale, then output a warning detailing which index, texture, material is having an issue.
		if (!bSuccess)
		{
			UTexture* FailedTexture = nullptr;
			for (int32 Index = 0; Index < Indices.Num() && !FailedTexture; ++Index)
			{
				for (int32 SubIndex : Indices[Index])
				{
					if (SubIndex == RegisterIndex)
					{
						FailedTexture = Textures[Index];
						break;
					}
				}
			}

			OutErrors.Register(InMaterial, FailedTexture, RegisterIndex, TextureIndexScales.Num() ? FExportErrorManager::EErrorType::EET_IncohorentValues : FExportErrorManager::EErrorType::EET_NoValues);
		}
	}

	return true;
}

bool FMaterialUtilities::ExportMaterial(struct FMaterialMergeData& InMaterialData, FFlattenMaterial& OutFlattenMaterial, struct FExportMaterialProxyCache* ProxyCache)
{
	UMaterialInterface* Material = InMaterialData.Material;
	UE_LOG(LogMaterialUtilities, Log, TEXT("Flattening material: %s"), *Material->GetName());

	if (ProxyCache)
	{
		// ExportMaterial was called with non-null CompiledMaterial. This means compiled shaders
		// should be stored outside, and could be re-used in next call to ExportMaterial.
		// FMaterialData already has "proxy cache" fiels, should swap it with CompiledMaterial,
		// and swap back before returning from this function.
		// Purpose of the following line: use compiled material cached from previous call.
		Exchange(ProxyCache, InMaterialData.ProxyCache);
	}
	
	// Precache all used textures, otherwise could get everything rendered with low-res textures.
	TArray<UTexture*> MaterialTextures;
	Material->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);

	for (UTexture* Texture : MaterialTextures)
	{
		if (Texture != NULL)
		{
			UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
			if (Texture2D)
			{
				Texture2D->SetForceMipLevelsToBeResident(30.0f, true);
				Texture2D->WaitForStreaming();
			}
		}		
	}

	// Determine whether or not certain properties can be rendered
	const bool bRenderNormal = (Material->GetMaterial()->HasNormalConnected() || Material->GetMaterial()->bUseMaterialAttributes) && OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Normal);
	const bool bRenderEmissive = (Material->GetMaterial()->EmissiveColor.IsConnected() || Material->GetMaterial()->bUseMaterialAttributes) && OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Emissive);
	const bool bRenderOpacityMask = Material->IsPropertyActive(MP_OpacityMask) && Material->GetBlendMode() == BLEND_Masked && OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Opacity);
	const bool bRenderOpacity = Material->IsPropertyActive(MP_Opacity) && IsTranslucentBlendMode(Material->GetBlendMode()) && OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Opacity);
	const bool bRenderSubSurface = Material->IsPropertyActive(MP_SubsurfaceColor) && OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::SubSurface);
	const bool bRenderMetallic = Material->IsPropertyActive(MP_Metallic) && OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Metallic);
	const bool bRenderSpecular = Material->IsPropertyActive(MP_Specular) && OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Specular);
	const bool bRenderRoughness = Material->IsPropertyActive(MP_Roughness) && OutFlattenMaterial.ShouldGenerateDataForProperty(EFlattenMaterialProperties::Roughness);

	check(!bRenderOpacity || !bRenderOpacityMask);

	FIntPoint Size;
	// Compile shaders and render flatten material.	
	Size = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse);
	RenderMaterialPropertyToTexture(InMaterialData, MP_BaseColor, false, PF_B8G8R8A8, OutFlattenMaterial.RenderSize, Size, OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse));
	OutFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Diffuse, Size);

	if (bRenderMetallic)
	{
		Size = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic);
		RenderMaterialPropertyToTexture(InMaterialData, MP_Metallic, false, PF_B8G8R8A8, OutFlattenMaterial.RenderSize, Size, OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic));
		OutFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Metallic, Size);
	}
	if (bRenderSpecular)
	{
		Size = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular);
		RenderMaterialPropertyToTexture(InMaterialData, MP_Specular, false, PF_B8G8R8A8, OutFlattenMaterial.RenderSize, Size, OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular));
		OutFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Specular, Size);
	}
	if (bRenderRoughness)
	{
		Size = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness);
		RenderMaterialPropertyToTexture(InMaterialData, MP_Roughness, false, PF_B8G8R8A8, OutFlattenMaterial.RenderSize, Size, OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness));
		OutFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Roughness, Size);
	}
	if (bRenderNormal)
	{
		Size = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal);
		RenderMaterialPropertyToTexture(InMaterialData, MP_Normal, true, PF_B8G8R8A8, OutFlattenMaterial.RenderSize, Size, OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal));
		OutFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Normal, Size);
	}
	if (bRenderOpacityMask)
	{
		Size = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity);
		RenderMaterialPropertyToTexture(InMaterialData, MP_OpacityMask, true, PF_B8G8R8A8, OutFlattenMaterial.RenderSize, Size, OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity));
		OutFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Opacity, Size);
	}
	if (bRenderOpacity)
	{
		Size = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity);
		// Number of blend modes, let's UMaterial decide whether it wants this property
		RenderMaterialPropertyToTexture(InMaterialData, MP_Opacity, true, PF_B8G8R8A8, OutFlattenMaterial.RenderSize, Size, OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity));
		OutFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Opacity, Size);
	}
	if (bRenderEmissive)
	{
		Size = OutFlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive);
		// PF_FloatRGBA is here to be able to render and read HDR image using ReadFloat16Pixels()
		RenderMaterialPropertyToTexture(InMaterialData, MP_EmissiveColor, false, PF_FloatRGBA, OutFlattenMaterial.RenderSize, Size, OutFlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive));
		OutFlattenMaterial.EmissiveScale = InMaterialData.EmissiveScale;
		OutFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Emissive, Size);
	}

	if (bRenderSubSurface)
	{
		// TODO support rendering out sub surface color property
		/*RenderMaterialPropertyToTexture(InMaterialData, MP_SubsurfaceColor, false, PF_B8G8R8A8, OutFlattenMaterial.RenderSize, OutFlattenMaterial.SubSurfaceSize, OutFlattenMaterial.SubSurfaceSamples);*/
	}

	OutFlattenMaterial.MaterialId = Material->GetLightingGuid();

	// Swap back the proxy cache
	if (ProxyCache)
	{
		// Store compiled material to external cache.
		Exchange(ProxyCache, InMaterialData.ProxyCache);
	}

	UE_LOG(LogMaterialUtilities, Log, TEXT("Material flattening done. (%s)"), *Material->GetName());

	return true;
}

bool FMaterialUtilities::RenderMaterialPropertyToTexture(struct FMaterialMergeData& InMaterialData, EMaterialProperty InMaterialProperty, bool bInForceLinearGamma, EPixelFormat InPixelFormat, const FIntPoint InTargetSize, FIntPoint& OutSampleSize, TArray<FColor>& OutSamples)
{
	if (InTargetSize.X == 0 || InTargetSize.Y == 0)
	{
		return false;
	}

	OutSampleSize = InTargetSize;

	FMaterialRenderProxy* MaterialProxy = nullptr;

	check(InMaterialProperty >= 0 && InMaterialProperty < ARRAY_COUNT(InMaterialData.ProxyCache->Proxies));
	if (InMaterialData.ProxyCache->Proxies[InMaterialProperty])
	{
		MaterialProxy = InMaterialData.ProxyCache->Proxies[InMaterialProperty];
	}
	else
	{
		MaterialProxy = InMaterialData.ProxyCache->Proxies[InMaterialProperty] = new FExportMaterialProxy(InMaterialData.Material, InMaterialProperty);
	}
	
	if (MaterialProxy == nullptr)
	{
		return false;
	}
	
	// Disallow garbage collection of RenderTarget.
	check(CurrentlyRendering == false);
	CurrentlyRendering = true;

	const bool bNormalMap = (InMaterialProperty == MP_Normal);
	UTextureRenderTarget2D* RenderTarget = CreateRenderTarget(bInForceLinearGamma, bNormalMap, InPixelFormat, OutSampleSize);
	OutSamples.Empty(InTargetSize.X * InTargetSize.Y);
	bool bResult = FMeshRenderer::RenderMaterial(
		InMaterialData,
		MaterialProxy,
		InMaterialProperty,
		RenderTarget,
		OutSamples);

	/** Disabled for now, see comment below */
	// Check for uniform value, perhaps this can be determined before rendering the material, see WillGenerateUniformData (LightmassRender)
	/*bool bIsUniform = true;
	FColor MaxColor(0, 0, 0, 0);
	if (bResult)
	{
		// Find maximal color value
		int32 MaxColorValue = 0;
		for (int32 Index = 0; Index < OutSamples.Num(); Index++)
		{
			FColor Color = OutSamples[Index];
			int32 ColorValue = Color.R + Color.G + Color.B + Color.A;
			if (ColorValue > MaxColorValue)
			{
				MaxColorValue = ColorValue;
				MaxColor = Color;
			}
		}

		// Fill background with maximal color value and render again		
		RenderTarget->ClearColor = FLinearColor(MaxColor);
		TArray<FColor> OutSamples2;
		FMeshRenderer::RenderMaterial(
			InMaterialData,
			MaterialProxy,
			InMaterialProperty,
			RenderTarget,
			OutSamples2);
		for (int32 Index = 0; Index < OutSamples2.Num(); Index++)
		{
			FColor Color = OutSamples2[Index];
			if (Color != MaxColor)
			{
				bIsUniform = false;
				break;
			}
		}
	}

	// Uniform value
	if (bIsUniform)
	{
		OutSampleSize = FIntPoint(1, 1);
		OutSamples.Empty();
		OutSamples.Add(MaxColor);
	}*/

	CurrentlyRendering = false;

	return bResult;
}

UTextureRenderTarget2D* FMaterialUtilities::CreateRenderTarget(bool bInForceLinearGamma, bool bNormalMap, EPixelFormat InPixelFormat, FIntPoint& InTargetSize)
{
	const FLinearColor ClearColour = bNormalMap ? FLinearColor(0.0f, 0.0f, 0.0f, 0.0f) : FLinearColor(1.0f, 0.0f, 1.0f, 0.0f);
	
	// Find any pooled render target with suitable properties.
	for (int32 RTIndex = 0; RTIndex < RenderTargetPool.Num(); RTIndex++)
	{
		UTextureRenderTarget2D* RenderTarget = RenderTargetPool[RTIndex];
		if (RenderTarget->SizeX == InTargetSize.X &&
			RenderTarget->SizeY == InTargetSize.Y &&
			RenderTarget->OverrideFormat == InPixelFormat &&
			RenderTarget->bForceLinearGamma == bInForceLinearGamma &&
			RenderTarget->ClearColor == RenderTarget->ClearColor )
		{
			return RenderTarget;
		}
	}

	// Not found - create a new one.
	UTextureRenderTarget2D* NewRenderTarget = NewObject<UTextureRenderTarget2D>();
	check(NewRenderTarget);
	NewRenderTarget->AddToRoot();
	NewRenderTarget->ClearColor = ClearColour;
	NewRenderTarget->TargetGamma = 0.0f;
	NewRenderTarget->InitCustomFormat(InTargetSize.X, InTargetSize.Y, InPixelFormat, bInForceLinearGamma);

	RenderTargetPool.Add(NewRenderTarget);
	return NewRenderTarget;
}

void FMaterialUtilities::ClearRenderTargetPool()
{
	if (CurrentlyRendering)
	{
		// Just in case - if garbage collection will happen during rendering, don't allow to GC used render target.
		return;
	}

	// Allow garbage collecting of all render targets.
	for (int32 RTIndex = 0; RTIndex < RenderTargetPool.Num(); RTIndex++)
	{
		RenderTargetPool[RTIndex]->RemoveFromRoot();
	}
	RenderTargetPool.Empty();
}

void FMaterialUtilities::FullyLoadMaterialStatic(UMaterialInterface* Material)
{
	FLinkerLoad* Linker = Material->GetLinker();
	if (Linker == nullptr)
	{
		return;
	}
	// Load material and all expressions.
	Linker->LoadAllObjects(true);
	Material->ConditionalPostLoad();
	// Now load all used textures.
	TArray<UTexture*> Textures;
	Material->GetUsedTextures(Textures, EMaterialQualityLevel::Num, true, ERHIFeatureLevel::SM5, true);
	for (UTexture* Texture : Textures)
	{
		Linker->Preload(Texture);
	}
}

void FMaterialUtilities::OptimizeSampleArray(TArray<FColor>& InSamples, FIntPoint& InSampleSize)
{
	if (InSamples.Num() > 1)
	{
		const FColor ColourValue = InSamples[0];
		bool bConstantValue = true;

		for (FColor& Sample : InSamples)
		{
			if (ColourValue != Sample)
			{
				bConstantValue = false;
				break;
			}
		}

		if (bConstantValue)
		{
			InSamples.Empty();
			InSamples.Add(ColourValue);
			InSampleSize = FIntPoint(1, 1);
		}
	}
}

FExportMaterialProxyCache::FExportMaterialProxyCache()
{
	FMemory::Memzero(Proxies);
}

FExportMaterialProxyCache::~FExportMaterialProxyCache()
{
	Release();
}

void FExportMaterialProxyCache::Release()
{
	for (int32 PropertyIndex = 0; PropertyIndex < ARRAY_COUNT(Proxies); PropertyIndex++)
	{
		FMaterialRenderProxy* Proxy = Proxies[PropertyIndex];
		if (Proxy)
		{
			delete Proxy;
			Proxies[PropertyIndex] = nullptr;
		}
	}
}


void FMaterialUtilities::DetermineMaterialImportance(const TArray<UMaterialInterface*>& InMaterials, TArray<float>& OutImportance)
{
	TArray<int32> MaterialImportance;
	int32 SummedSize = 0;
	for (UMaterialInterface* Material : InMaterials)
	{
		TArray<UTexture*> UsedTextures;
		Material->GetMaterial()->AppendReferencedTextures(UsedTextures);
		if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material))
		{
			for (const FTextureParameterValue& TextureParameter : MaterialInstance->TextureParameterValues)
			{
				if (TextureParameter.ParameterValue != nullptr)
				{
					UsedTextures.Add(TextureParameter.ParameterValue);
				}
			}
		}
		int32 MaxSize = 64 * 64;
		for (UTexture* Texture : UsedTextures)
		{
			if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
			{

				// In game max bias and dimensions
				uint32 MaxInGameWidth, MaxInGameHeight;
				int32 MipLevel = UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->CalculateLODBias(Texture2D);
				//Calculate in-game max resolution and store in EffectiveTextureWidth, EffectiveTextureHeight
				UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->ComputeInGameMaxResolution(MipLevel, *Texture, (uint32 &)MaxInGameWidth, (uint32 &)MaxInGameHeight);
				
				int32 TotalSize = MaxInGameWidth * MaxInGameHeight;
				if (TotalSize > MaxSize)
				{
					MaxSize = TotalSize;
				}
			}
		}
		
		MaterialImportance.Add(MaxSize);
		SummedSize += MaxSize;
	}
	float WeightPerPixel = 1.0f / SummedSize;
	for (int32 MaterialIndex = 0; MaterialIndex < InMaterials.Num(); ++MaterialIndex)
	{
		OutImportance.Add((float)MaterialImportance[MaterialIndex] * WeightPerPixel);
	}	
}

void FMaterialUtilities::GeneratedBinnedTextureSquares(const FVector2D DestinationSize, TArray<float>& InTexureWeights, TArray<FBox2D>& OutGeneratedBoxes)
{
	typedef FBox2D FTextureArea;
	struct FWeightedTexture
	{
		FTextureArea Area;
		int32 TextureIndex;
		float Weight;
	};

	TArray<FWeightedTexture> WeightedTextures;
	const float TotalArea = DestinationSize.X * DestinationSize.Y;
	// Generate textures with their size calculated according to their weight
	for (int32 WeightIndex = 0; WeightIndex < InTexureWeights.Num(); ++WeightIndex)
	{
		const float Weight = InTexureWeights[WeightIndex];
		FWeightedTexture Texture;
		float TextureSize = FMath::Sqrt(TotalArea*Weight);
		Texture.Area = FTextureArea( FVector2D(0.0f,0.0f), FVector2D(TextureSize, TextureSize));
		Texture.TextureIndex = WeightIndex;
		Texture.Weight = Weight;
		WeightedTextures.Add(Texture);
	}

	// Sort textures by their weight (high to low) which influences the insert order
	WeightedTextures.Sort([](const FWeightedTexture& One, const FWeightedTexture& Two) { return One.Weight > Two.Weight; });
		
	TArray<FWeightedTexture> InsertedTextures;
	typedef FBox2D FUnusedArea;
	TArray<FUnusedArea> UnusedAreas;

	bool bSuccess = true;
	do 
	{
		// Reset state
		bSuccess = true;
		UnusedAreas.Empty();
		InsertedTextures.Empty();
		FUnusedArea StartArea(FVector2D(0, 0), DestinationSize);
		UnusedAreas.Add(StartArea);

		for (const FWeightedTexture& Texture : WeightedTextures)
		{
			int32 BestAreaIndex = -1;
			float RemainingArea = FLT_MAX;
			FVector2D TextureSize = Texture.Area.GetSize();
			float TextureSurface = TextureSize.X * TextureSize.Y;

			// Find best area to insert this texture in (determined by tightest fit)
			for (int32 AreaIndex = 0; AreaIndex < UnusedAreas.Num(); ++AreaIndex)
			{
				const FUnusedArea& UnusedArea = UnusedAreas[AreaIndex];
				if (UnusedArea.GetSize() >= TextureSize)
				{
					const float Remainder = UnusedArea.GetArea() - TextureSurface;
					if (Remainder < RemainingArea && Remainder >= 0)
					{
						BestAreaIndex = AreaIndex;
						RemainingArea = Remainder;
					}
				}
			}

			// Insert the texture in case we found an appropriate area
			if (BestAreaIndex != -1)
			{
				FUnusedArea& UnusedArea = UnusedAreas[BestAreaIndex];
				FVector2D UnusedSize = UnusedArea.GetSize();

				// Push back texture
				FWeightedTexture WeightedTexture;
				WeightedTexture.Area = FTextureArea(UnusedArea.Min, UnusedArea.Min + TextureSize);
				WeightedTexture.TextureIndex = Texture.TextureIndex;
				InsertedTextures.Add(WeightedTexture);
				
				// Generate two new resulting unused areas from splitting up the result
				/*
					___________
					|	  |   |
					|	  | V |
					|_____|   |
					|  H  |   |
					|_____|___|
				*/
				FUnusedArea HorizontalArea, VerticalArea;
				HorizontalArea.Min.X = UnusedArea.Min.X;
				HorizontalArea.Min.Y = UnusedArea.Min.Y + TextureSize.Y;
				HorizontalArea.Max.X = HorizontalArea.Min.X + TextureSize.X;
				HorizontalArea.Max.Y = HorizontalArea.Min.Y + (UnusedSize.Y - TextureSize.Y);

				VerticalArea.Min.X = UnusedArea.Min.X + TextureSize.X;
				VerticalArea.Min.Y = UnusedArea.Min.Y;
				VerticalArea.Max.X = VerticalArea.Min.X + (UnusedSize.X - TextureSize.X);
				VerticalArea.Max.Y = UnusedSize.Y;

				// Append valid new areas to list (replace original one with either one of the new ones)
				const bool bValidHorizontal = HorizontalArea.GetArea() > 0.0f;
				const bool bValidVertical = VerticalArea.GetArea() > 0.0f;
				if (bValidVertical && bValidHorizontal)
				{
					UnusedAreas[BestAreaIndex] = HorizontalArea;
					UnusedAreas.Add(VerticalArea);
				}
				else if (bValidVertical)
				{
					UnusedAreas[BestAreaIndex] = VerticalArea;
				}
				else if (bValidHorizontal)
				{
					UnusedAreas[BestAreaIndex] = HorizontalArea;
				}
			}
			else
			{
				bSuccess = false;
				break;
			}
		}

		// This means we failed to find a fit, in this case we resize the textures and try again until we find one
		if (bSuccess == false)
		{
			for (FWeightedTexture& Texture : WeightedTextures)
			{
				Texture.Area.Max *= .99f;
			}
		}
	} while (!bSuccess);

	// Now generate boxes
	OutGeneratedBoxes.Empty(InTexureWeights.Num());
	OutGeneratedBoxes.AddZeroed(InTexureWeights.Num());

	// Generate boxes according to the inserted textures
	for (const FWeightedTexture& Texture : InsertedTextures)
	{
		FBox2D& Box = OutGeneratedBoxes[Texture.TextureIndex];
		Box = Texture.Area;
	}
}

