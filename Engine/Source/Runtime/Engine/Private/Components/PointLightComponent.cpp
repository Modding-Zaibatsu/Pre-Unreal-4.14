// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PointLightComponent.cpp: PointLightComponent implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "PointLightSceneProxy.h"

static int32 GAllowPointLightCubemapShadows = 1;
static FAutoConsoleVariableRef CVarAllowPointLightCubemapShadows(
	TEXT("r.AllowPointLightCubemapShadows"),
	GAllowPointLightCubemapShadows,
	TEXT("When 0, will prevent point light cube map shadows from being used and the light will be unshadowed.")
	);

/**
 * The point light policy for TMeshLightingDrawingPolicy.
 */
class FPointLightPolicy
{
public:
	typedef TPointLightSceneProxy<FPointLightPolicy> SceneInfoType;
};

void FPointLightSceneProxyBase::UpdateRadius_GameThread(UPointLightComponent* Component)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		UpdateRadius,
		FPointLightSceneProxyBase*,LightSceneInfo,this,
		float,ComponentRadius,Component->AttenuationRadius,
	{
		LightSceneInfo->UpdateRadius(ComponentRadius);
	});
}

class FPointLightSceneProxy : public TPointLightSceneProxy<FPointLightPolicy>
{
public:

	/** Accesses parameters needed for rendering the light. */
	virtual void GetParameters(FVector4& LightPositionAndInvRadius, FVector4& LightColorAndFalloffExponent, FVector& NormalizedLightDirection, FVector2D& SpotAngles, float& LightSourceRadius, float& LightSourceLength, float& LightMinRoughness) const
	{
		LightPositionAndInvRadius = FVector4(
			GetOrigin(),
			InvRadius);

		LightColorAndFalloffExponent = FVector4(
			GetColor().R,
			GetColor().G,
			GetColor().B,
			FalloffExponent);

		NormalizedLightDirection = -GetDirection();
		SpotAngles = FVector2D( -2.0f, 1.0f );
		LightSourceRadius = SourceRadius;
		LightSourceLength = SourceLength;
		// Prevent 0 Roughness which causes NaNs in Vis_SmithJointApprox
		LightMinRoughness = FMath::Max(MinRoughness, .04f);
	}

	virtual FSphere GetBoundingSphere() const
	{
		return FSphere(GetPosition(), GetRadius());
	}

	/**
	 * Sets up a projected shadow initializer for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const
	{
		if (ViewFamily.GetFeatureLevel() >= ERHIFeatureLevel::SM4
			&& GAllowPointLightCubemapShadows != 0)
		{
			FWholeSceneProjectedShadowInitializer& OutInitializer = *new(OutInitializers) FWholeSceneProjectedShadowInitializer;
			OutInitializer.PreShadowTranslation = -GetLightToWorld().GetOrigin();
			OutInitializer.WorldToLight = GetWorldToLight().RemoveTranslation();
			OutInitializer.Scales = FVector(1, 1, 1);
			OutInitializer.FaceDirection = FVector(0,0,1);
			OutInitializer.SubjectBounds = FBoxSphereBounds(FVector(0, 0, 0),FVector(Radius,Radius,Radius),Radius);
			OutInitializer.WAxis = FVector4(0,0,1,0);
			OutInitializer.MinLightW = 0.1f;
			OutInitializer.MaxDistanceToCastInLightW = Radius;
			OutInitializer.bOnePassPointLightShadow = true;
			OutInitializer.bRayTracedDistanceField = UseRayTracedDistanceFieldShadows() && DoesPlatformSupportDistanceFieldShadowing(ViewFamily.GetShaderPlatform());
			return true;
		}
		
		return false;
	}

	FPointLightSceneProxy(const UPointLightComponent* Component)
	:	TPointLightSceneProxy<FPointLightPolicy>(Component)
	{}
};

UPointLightComponent::UPointLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightPoint"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> DynamicTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightPointMove"));

		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 0.5f;
		DynamicEditorTexture = DynamicTexture.Object;
		DynamicEditorTextureScale = 0.5f;
	}
#endif

	Intensity = 5000;
	Radius_DEPRECATED = 1024.0f;
	AttenuationRadius = 1000;
	LightFalloffExponent = 8.0f;
	SourceRadius = 0.0f;
	SourceLength = 0.0f;
	bUseInverseSquaredFalloff = true;
}

FLightSceneProxy* UPointLightComponent::CreateSceneProxy() const
{
	return new FPointLightSceneProxy(this);
}

void UPointLightComponent::SetAttenuationRadius(float NewRadius)
{
	// Only movable lights can change their radius at runtime
	if (AreDynamicDataChangesAllowed(false)
		&& NewRadius != AttenuationRadius)
	{
		AttenuationRadius = NewRadius;
		PushRadiusToRenderThread();
	}
}

void UPointLightComponent::SetLightFalloffExponent(float NewLightFalloffExponent)
{
	if (AreDynamicDataChangesAllowed()
		&& NewLightFalloffExponent != LightFalloffExponent)
	{
		LightFalloffExponent = NewLightFalloffExponent;
		MarkRenderStateDirty();
	}
}

void UPointLightComponent::SetSourceRadius(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& SourceRadius != NewValue)
	{
		SourceRadius = NewValue;
		MarkRenderStateDirty();
	}
}

void UPointLightComponent::SetSourceLength(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& SourceLength != NewValue)
	{
		SourceLength = NewValue;
		MarkRenderStateDirty();
	}
}

bool UPointLightComponent::AffectsBounds(const FBoxSphereBounds& InBounds) const
{
	if((InBounds.Origin - ComponentToWorld.GetLocation()).SizeSquared() > FMath::Square(AttenuationRadius + InBounds.SphereRadius))
	{
		return false;
	}

	if(!Super::AffectsBounds(InBounds))
	{
		return false;
	}

	return true;
}

void UPointLightComponent::SendRenderTransform_Concurrent()
{
	// Update the scene info's cached radius-dependent data.
	if(SceneProxy)
	{
		((FPointLightSceneProxyBase*)SceneProxy)->UpdateRadius_GameThread(this);
	}

	Super::SendRenderTransform_Concurrent();
}

//
FVector4 UPointLightComponent::GetLightPosition() const
{
	return FVector4(ComponentToWorld.GetLocation(),1);
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType UPointLightComponent::GetLightType() const
{
	return LightType_Point;
}

float UPointLightComponent::GetUniformPenumbraSize() const
{
	if (LightmassSettings.bUseAreaShadowsForStationaryLight)
	{
		// Interpret distance as shadow factor directly
		return 1.0f;
	}
	else
	{
		// Heuristic to derive uniform penumbra size from light source radius
		return FMath::Clamp(SourceRadius == 0 ? .05f : SourceRadius * .005f, .0001f, 1.0f);
	}
}

//
FBox UPointLightComponent::GetBoundingBox() const
{
	return FBox(GetComponentLocation() - FVector(AttenuationRadius,AttenuationRadius,AttenuationRadius),GetComponentLocation() + FVector(AttenuationRadius,AttenuationRadius,AttenuationRadius));
}

FSphere UPointLightComponent::GetBoundingSphere() const
{
	return FSphere(ComponentToWorld.GetLocation(), AttenuationRadius);
}

void UPointLightComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_INVERSE_SQUARED_LIGHTS_DEFAULT)
	{
		bUseInverseSquaredFalloff = InverseSquaredFalloff_DEPRECATED;
		AttenuationRadius = Radius_DEPRECATED;
	}
}

#if WITH_EDITOR

bool UPointLightComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bCastShadowsFromCinematicObjectsOnly) && bUseRayTracedDistanceFieldShadows)
		{
			return false;
		}

		if (FCString::Strcmp(*PropertyName, TEXT("LightFalloffExponent")) == 0)
		{
			return !bUseInverseSquaredFalloff;
		}
	}

	return Super::CanEditChange(InProperty);
}

/**
 * Called after property has changed via e.g. property window or set command.
 *
 * @param	PropertyThatChanged	UProperty that has been changed, NULL if unknown
 */
void UPointLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Make sure exponent is > 0.
	LightFalloffExponent = FMath::Max( (float) KINDA_SMALL_NUMBER, LightFalloffExponent );
	SourceRadius = FMath::Max(0.0f, SourceRadius);
	SourceLength = FMath::Max(0.0f, SourceLength);
	Intensity = FMath::Max(0.0f, Intensity);
	LightmassSettings.IndirectLightingSaturation = FMath::Max(LightmassSettings.IndirectLightingSaturation, 0.0f);
	LightmassSettings.ShadowExponent = FMath::Clamp(LightmassSettings.ShadowExponent, .5f, 8.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UPointLightComponent::PostInterpChange(UProperty* PropertyThatChanged)
{
	static FName RadiusName(TEXT("Radius"));
	static FName AttenuationRadiusName(TEXT("AttenuationRadius"));
	static FName LightFalloffExponentName(TEXT("LightFalloffExponent"));
	FName PropertyName = PropertyThatChanged->GetFName();

	if (PropertyName == RadiusName
		|| PropertyName == AttenuationRadiusName)
	{
		// Old radius tracks will animate the deprecated value
		if (PropertyName == RadiusName)
		{
			AttenuationRadius = Radius_DEPRECATED;
		}

		PushRadiusToRenderThread();
	}
	else if (PropertyName == LightFalloffExponentName)
	{
		MarkRenderStateDirty();
	}
	else
	{
		Super::PostInterpChange(PropertyThatChanged);
	}
}

void UPointLightComponent::PushRadiusToRenderThread()
{
	if (CastShadows)
	{
		// Shadow casting lights need to recompute light interactions
		// to determine which primitives to draw in shadow depth passes.
		MarkRenderStateDirty();
	}
	else
	{
		if (SceneProxy)
		{
			((FPointLightSceneProxyBase*)SceneProxy)->UpdateRadius_GameThread(this);
		}
	}
}

