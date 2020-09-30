// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneView.cpp: SceneView implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "SceneView.h"
#include "SceneManagement.h"
#include "EngineModule.h"
#include "StereoRendering.h"
#include "HighResScreenshot.h"
#include "Slate/SceneViewport.h"
#include "RendererInterface.h"
#include "BufferVisualizationData.h"
#include "Interfaces/Interface_PostProcessVolume.h"
#include "Engine/TextureCube.h"
#include "IHeadMountedDisplay.h"
#include "Classes/Engine/RendererSettings.h"
#include "LightPropagationVolumeBlendable.h"
#include "SceneViewExtension.h"

#if WITH_EDITOR
	#include "UnrealEd.h"
#endif

DEFINE_LOG_CATEGORY(LogBufferVisualization);

DECLARE_CYCLE_STAT(TEXT("StartFinalPostprocessSettings"), STAT_StartFinalPostprocessSettings, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("OverridePostProcessSettings"), STAT_OverridePostProcessSettings, STATGROUP_Engine);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FPrimitiveUniformShaderParameters,TEXT("Primitive"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FViewUniformShaderParameters,TEXT("View"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FInstancedViewUniformShaderParameters, TEXT("InstancedView"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FBuiltinSamplersParameters, TEXT("BuiltinSamplers"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FMobileDirectionalLightShaderParameters, TEXT("MobileDirectionalLight"));

FBuiltinSamplersUniformBuffer::FBuiltinSamplersUniformBuffer()
{
	FBuiltinSamplersParameters UB;
	UB.Bilinear = nullptr;
	UB.BilinearClamped = nullptr;
	UB.Point = nullptr;
	UB.PointClamped = nullptr;
	UB.Trilinear = nullptr;
	UB.TrilinearClamped = nullptr;
	SetContents(UB);
}

static TRefCountPtr<FRHISamplerState> BuiltinBilinear;
static TRefCountPtr<FRHISamplerState> BuiltinBilinearClamped;
static TRefCountPtr<FRHISamplerState> BuiltinPoint;
static TRefCountPtr<FRHISamplerState> BuiltinPointClamped;
static TRefCountPtr<FRHISamplerState> BuiltinTrilinear;
static TRefCountPtr<FRHISamplerState> BuiltinTrilinearClamped;

void FBuiltinSamplersUniformBuffer::InitDynamicRHI()
{
	BuiltinBilinear =			RHICreateSamplerState(FSamplerStateInitializerRHI(SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap));
	BuiltinBilinearClamped =	RHICreateSamplerState(FSamplerStateInitializerRHI(SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp));
	BuiltinPoint =				RHICreateSamplerState(FSamplerStateInitializerRHI(SF_Point, AM_Wrap, AM_Wrap, AM_Wrap));
	BuiltinPointClamped =		RHICreateSamplerState(FSamplerStateInitializerRHI(SF_Point, AM_Clamp, AM_Clamp, AM_Clamp));
	BuiltinTrilinear =			RHICreateSamplerState(FSamplerStateInitializerRHI(SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap));
	BuiltinTrilinearClamped =	RHICreateSamplerState(FSamplerStateInitializerRHI(SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp));

	FBuiltinSamplersParameters UB;

	UB.Bilinear =			BuiltinBilinear;
	UB.BilinearClamped =	BuiltinBilinearClamped;
	UB.Point =				BuiltinPoint;
	UB.PointClamped =		BuiltinPointClamped;
	UB.Trilinear =			BuiltinTrilinear;
	UB.TrilinearClamped =	BuiltinTrilinearClamped;
	SetContents(UB);

	TUniformBuffer<FBuiltinSamplersParameters>::InitDynamicRHI();
}

void FBuiltinSamplersUniformBuffer::ReleaseDynamicRHI()
{
	TUniformBuffer<FBuiltinSamplersParameters>::ReleaseDynamicRHI();

	BuiltinBilinear =			nullptr;
	BuiltinBilinearClamped =	nullptr;
	BuiltinPoint =				nullptr;
	BuiltinPointClamped =		nullptr;
	BuiltinTrilinear =			nullptr;
	BuiltinTrilinearClamped =	nullptr;
}

TGlobalResource<FBuiltinSamplersUniformBuffer> GBuiltinSamplersUniformBuffer;


static TAutoConsoleVariable<float> CVarSSRMaxRoughness(
	TEXT("r.SSR.MaxRoughness"),
	-1.0f,
	TEXT("Allows to override the post process setting ScreenSpaceReflectionMaxRoughness.\n")
	TEXT("It defines until what roughness we fade the screen space reflections, 0.8 works well, smaller can run faster.\n")
	TEXT("(Useful for testing, no scalability or project setting)\n")
	TEXT(" 0..1: use specified max roughness (overrride PostprocessVolume setting)\n")
	TEXT(" -1: no override (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)


static TAutoConsoleVariable<int32> CVarShadowFreezeCamera(
	TEXT("r.Shadow.FreezeCamera"),
	0,
	TEXT("Debug the shadow methods by allowing to observe the system from outside.\n")
	TEXT("0: default\n")
	TEXT("1: freeze camera at current location"),
	ECVF_Cheat);

static TAutoConsoleVariable<float> CVarExposureOffset(
	TEXT("r.ExposureOffset"),
	0.0f,
	TEXT("For adjusting the exposure on top of post process settings and eye adaptation. For developers only. 0:default"),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarRenderTimeFrozen(
	TEXT("r.RenderTimeFrozen"),
	0,
	TEXT("Allows to freeze time based effects in order to provide more deterministic render profiling.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on (Note: this also disables occlusion queries)"),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarScreenPercentageEditor(
	TEXT("r.ScreenPercentage.Editor"),
	0,
	TEXT("To allow to have an effect of ScreenPercentage in the editor.\n")
	TEXT("0: off (default)\n")
	TEXT("1: allow upsample (blurry but faster) and downsample (cripser but slower)"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarDepthOfFieldDepthBlurAmount(
	TEXT("r.DepthOfField.DepthBlur.Amount"),
	1.0f,
	TEXT("This scale multiplier only affects the CircleDOF DepthBlur feature (value defines in how many km the radius goes to 50%).\n")
	TEXT(" x: Multiply the existing Depth Blur Amount with x\n")
	TEXT("-x: Override the existing Depth Blur Amount with x (in km)\n")
	TEXT(" 1: No adjustments (default)"),
	ECVF_RenderThreadSafe | ECVF_Cheat);

static TAutoConsoleVariable<float> CVarDepthOfFieldDepthBlurScale(
	TEXT("r.DepthOfField.DepthBlur.Scale"),
	1.0f,
	TEXT("This scale multiplier only affects the CircleDOF DepthBlur feature. This is applied after r.DepthOfField.DepthBlur.ResolutionScale.\n")
	TEXT(" 0: Disable Depth Blur\n")
	TEXT(" x: Multiply the existing Depth Blur Radius with x\n")
	TEXT("-x: Override the existing Depth Blur Radius with x\n")
	TEXT(" 1: No adjustments (default)"),
	ECVF_RenderThreadSafe | ECVF_Cheat);

static TAutoConsoleVariable<float> CVarDepthOfFieldDepthBlurResolutionScale(
	TEXT("r.DepthOfField.DepthBlur.ResolutionScale"),
	1.0f,
	TEXT("This scale multiplier only affects the CircleDOF DepthBlur feature. It's a temporary hack.\n")
	TEXT("It lineary scale the DepthBlur by the resolution increase over 1920 (in width), does only affect resolution larger than that.\n")
	TEXT("Actual math: float Factor = max(ViewWidth / 1920 - 1, 0); DepthBlurRadius *= 1 + Factor * (CVar - 1)\n")
	TEXT(" 1: No adjustments (default)\n")
	TEXT(" x: if the resolution is 1920 there is no change, if 2x larger than 1920 it scale the radius by x"),
	ECVF_RenderThreadSafe | ECVF_Cheat);
#endif

static TAutoConsoleVariable<float> CVarSSAOFadeRadiusScale(
	TEXT("r.AmbientOcclusion.FadeRadiusScale"),
	1.0f,
	TEXT("Allows to scale the ambient occlusion fade radius (SSAO).\n")
	TEXT(" 0.01:smallest .. 1.0:normal (default), <1:smaller, >1:larger"),
	ECVF_Cheat | ECVF_RenderThreadSafe);

// Engine default (project settings):

static TAutoConsoleVariable<int32> CVarDefaultBloom(
	TEXT("r.DefaultFeature.Bloom"),
	1,
	TEXT("Engine default (project setting) for Bloom is (postprocess volume/camera/game setting still can override)\n")
	TEXT(" 0: off, set BloomIntensity to 0\n")
	TEXT(" 1: on (default)"));

static TAutoConsoleVariable<int32> CVarDefaultAmbientOcclusion(
	TEXT("r.DefaultFeature.AmbientOcclusion"),
	1,
	TEXT("Engine default (project setting) for AmbientOcclusion is (postprocess volume/camera/game setting still can override)\n")
	TEXT(" 0: off, sets AmbientOcclusionIntensity to 0\n")
	TEXT(" 1: on (default)"));

static TAutoConsoleVariable<int32> CVarDefaultAmbientOcclusionStaticFraction(
	TEXT("r.DefaultFeature.AmbientOcclusionStaticFraction"),
	1,
	TEXT("Engine default (project setting) for AmbientOcclusion is (postprocess volume/camera/game setting still can override)\n")
	TEXT(" 0: off, sets AmbientOcclusionStaticFraction to 0\n")
	TEXT(" 1: on (default, costs extra pass, only useful if there is some baked lighting)"));

static TAutoConsoleVariable<int32> CVarDefaultAutoExposure(
	TEXT("r.DefaultFeature.AutoExposure"),
	1,
	TEXT("Engine default (project setting) for AutoExposure is (postprocess volume/camera/game setting still can override)\n")
	TEXT(" 0: off, sets AutoExposureMinBrightness and AutoExposureMaxBrightness to 1\n")
	TEXT(" 1: on (default)"));

static TAutoConsoleVariable<int32> CVarDefaultAutoExposureMethod(
	TEXT("r.DefaultFeature.AutoExposure.Method"),
	0,
	TEXT("Engine default (project setting) for AutoExposure Method (postprocess volume/camera/game setting still can override)\n")
	TEXT(" 0: Histogram based (requires compute shader, default)\n")
	TEXT(" 1: Basic AutoExposure"));

static TAutoConsoleVariable<int32> CVarDefaultMotionBlur(
	TEXT("r.DefaultFeature.MotionBlur"),
	1,
	TEXT("Engine default (project setting) for MotionBlur is (postprocess volume/camera/game setting still can override)\n")
	TEXT(" 0: off, sets MotionBlurAmount to 0\n")
	TEXT(" 1: on (default)"));

// off by default for better performance and less distractions
static TAutoConsoleVariable<int32> CVarDefaultLensFlare(
	TEXT("r.DefaultFeature.LensFlare"),
	0,
	TEXT("Engine default (project setting) for LensFlare is (postprocess volume/camera/game setting still can override)\n")
	TEXT(" 0: off, sets LensFlareIntensity to 0\n")
	TEXT(" 1: on (default)"));

// see EAntiAliasingMethod
static TAutoConsoleVariable<int32> CVarDefaultAntiAliasing(
	TEXT("r.DefaultFeature.AntiAliasing"),
	2,
	TEXT("Engine default (project setting) for AntiAliasingMethod is (postprocess volume/camera/game setting still can override)\n")
	TEXT(" 0: off (no anti-aliasing)\n")
	TEXT(" 1: FXAA (faster than TemporalAA but much more shimmering for non static cases)\n")
	TEXT(" 2: TemporalAA (default)\n")
	TEXT(" 3: MSAA (Forward shading only)"));

static TAutoConsoleVariable<float> CVarMotionBlurScale(
	TEXT("r.MotionBlur.Scale"),
	1.0f,
	TEXT("Allows to scale the postprocess intensity/amount setting in the postprocess.\n")
	TEXT("1: don't do any scaling (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarMotionBlurAmount(
	TEXT("r.MotionBlur.Amount"),
	-1.0f,
	TEXT("Allows to override the postprocess setting (scale of motion blur)\n")
	TEXT("-1: override (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarMotionBlurMax(
	TEXT("r.MotionBlur.Max"),
	-1.0f,
	TEXT("Allows to override the postprocess setting (max length of motion blur, in percent of the screen width)\n")
	TEXT("-1: override (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSceneColorFringeMax(
	TEXT("r.SceneColorFringe.Max"),
	-1.0f,
	TEXT("Allows to clamp the postprocess setting (in percent, Scene chromatic aberration / color fringe to simulate an artifact that happens in real-world lens, mostly visible in the image corners)\n")
	TEXT("-1: don't clamp (default)\n")
	TEXT("-2: to test extreme fringe"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarTonemapperQuality(
	TEXT("r.Tonemapper.Quality"),
	5,
	TEXT("Defines the Tonemapper Quality in the range 0..5\n")
	TEXT("Depending on the used settings we might pick a faster shader permutation\n")
	TEXT(" 0: basic tonemapper only, lowest quality\n")
	TEXT(" 1: + FilmContrast\n")
	TEXT(" 2: + Vignette\n")
	TEXT(" 3: + FilmShadowTintAmount\n")
	TEXT(" 4: + Grain\n")
	TEXT(" 5: + GrainJitter = full quality (default)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarTessellationAdaptivePixelsPerTriangle(
	TEXT("r.TessellationAdaptivePixelsPerTriangle"),
	48.0f,
	TEXT("Global tessellation factor multiplier"),
	ECVF_RenderThreadSafe);

// should be changed to BaseColor and Metallic, since some time now UE4 is not using DiffuseColor and SpecularColor any more
static TAutoConsoleVariable<float> CVarDiffuseColorMin(
	TEXT("r.DiffuseColor.Min"),
	0.0f,
	TEXT("Allows quick material test by remapping the diffuse color at 1 to a new value (0..1), Only for non shipping built!\n")
	TEXT("1: (default)"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<float> CVarDiffuseColorMax(
	TEXT("r.DiffuseColor.Max"),
	1.0f,
	TEXT("Allows quick material test by remapping the diffuse color at 1 to a new value (0..1), Only for non shipping built!\n")
	TEXT("1: (default)"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<float> CVarRoughnessMin(
	TEXT("r.Roughness.Min"),
	0.0f,
	TEXT("Allows quick material test by remapping the roughness at 0 to a new value (0..1), Only for non shipping built!\n")
	TEXT("0: (default)"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<float> CVarRoughnessMax(
	TEXT("r.Roughness.Max"),
	1.0f,
	TEXT("Allows quick material test by remapping the roughness at 1 to a new value (0..1), Only for non shipping built!\n")
	TEXT("1: (default)"),
	ECVF_Cheat | ECVF_RenderThreadSafe
	);

/** Global vertex color view mode setting when SHOW_VertexColors show flag is set */
EVertexColorViewMode::Type GVertexColorViewMode = EVertexColorViewMode::Color;

/** Global primitive uniform buffer resource containing identity transformations. */
ENGINE_API TGlobalResource<FIdentityPrimitiveUniformBuffer> GIdentityPrimitiveUniformBuffer;

FSceneViewStateReference::~FSceneViewStateReference()
{
	Destroy();
}

void FSceneViewStateReference::Allocate()
{
	check(!Reference);
	Reference = GetRendererModule().AllocateViewState();
	GlobalListLink = TLinkedList<FSceneViewStateReference*>(this);
	GlobalListLink.LinkHead(GetSceneViewStateList());
}

void FSceneViewStateReference::Destroy()
{
	GlobalListLink.Unlink();

	if (Reference)
	{
		Reference->Destroy();
		Reference = NULL;
	}
}

void FSceneViewStateReference::DestroyAll()
{
	for(TLinkedList<FSceneViewStateReference*>::TIterator ViewStateIt(FSceneViewStateReference::GetSceneViewStateList());ViewStateIt;ViewStateIt.Next())
	{
		FSceneViewStateReference* ViewStateReference = *ViewStateIt;
		ViewStateReference->Reference->Destroy();
		ViewStateReference->Reference = NULL;
	}
}

void FSceneViewStateReference::AllocateAll()
{
	for(TLinkedList<FSceneViewStateReference*>::TIterator ViewStateIt(FSceneViewStateReference::GetSceneViewStateList());ViewStateIt;ViewStateIt.Next())
	{
		FSceneViewStateReference* ViewStateReference = *ViewStateIt;
		ViewStateReference->Reference = GetRendererModule().AllocateViewState();
	}
}

TLinkedList<FSceneViewStateReference*>*& FSceneViewStateReference::GetSceneViewStateList()
{
	static TLinkedList<FSceneViewStateReference*>* List = NULL;
	return List;
}

/**
 * Utility function to create the inverse depth projection transform to be used
 * by the shader system.
 * @param ProjMatrix - used to extract the scene depth ratios
 * @param InvertZ - projection calc is affected by inverted device Z
 * @return vector containing the ratios needed to convert from device Z to world Z
 */
FVector4 CreateInvDeviceZToWorldZTransform(const FMatrix& ProjMatrix)
{
	// The perspective depth projection comes from the the following projection matrix:
	//
	// | 1  0  0  0 |
	// | 0  1  0  0 |
	// | 0  0  A  1 |
	// | 0  0  B  0 |
	//
	// Z' = (Z * A + B) / Z
	// Z' = A + B / Z
	//
	// So to get Z from Z' is just:
	// Z = B / (Z' - A)
	// 
	// Note a reversed Z projection matrix will have A=0. 
	//
	// Done in shader as:
	// Z = 1 / (Z' * C1 - C2)   --- Where C1 = 1/B, C2 = A/B
	//

	float DepthMul = ProjMatrix.M[2][2];
	float DepthAdd = ProjMatrix.M[3][2];

	if (DepthAdd == 0.f)
	{
		// Avoid dividing by 0 in this case
		DepthAdd = 0.00000001f;
	}

	// perspective
	// SceneDepth = 1.0f / (DeviceZ / ProjMatrix.M[3][2] - ProjMatrix.M[2][2] / ProjMatrix.M[3][2])

	// ortho
	// SceneDepth = DeviceZ / ProjMatrix.M[2][2] - ProjMatrix.M[3][2] / ProjMatrix.M[2][2];

	// combined equation in shader to handle either
	// SceneDepth = DeviceZ * View.InvDeviceZToWorldZTransform[0] + View.InvDeviceZToWorldZTransform[1] + 1.0f / (DeviceZ * View.InvDeviceZToWorldZTransform[2] - View.InvDeviceZToWorldZTransform[3]);

	// therefore perspective needs
	// View.InvDeviceZToWorldZTransform[0] = 0.0f
	// View.InvDeviceZToWorldZTransform[1] = 0.0f
	// View.InvDeviceZToWorldZTransform[2] = 1.0f / ProjMatrix.M[3][2]
	// View.InvDeviceZToWorldZTransform[3] = ProjMatrix.M[2][2] / ProjMatrix.M[3][2]

	// and ortho needs
	// View.InvDeviceZToWorldZTransform[0] = 1.0f / ProjMatrix.M[2][2]
	// View.InvDeviceZToWorldZTransform[1] = -ProjMatrix.M[3][2] / ProjMatrix.M[2][2] + 1.0f
	// View.InvDeviceZToWorldZTransform[2] = 0.0f
	// View.InvDeviceZToWorldZTransform[3] = 1.0f

	bool bIsPerspectiveProjection = ProjMatrix.M[3][3] < 1.0f;

	if (bIsPerspectiveProjection)
	{
		float SubtractValue = DepthMul / DepthAdd;

		// Subtract a tiny number to avoid divide by 0 errors in the shader when a very far distance is decided from the depth buffer.
		// This fixes fog not being applied to the black background in the editor.
		SubtractValue -= 0.00000001f;

		return FVector4(
			0.0f,			
			0.0f,			
			1.0f / DepthAdd,	
			SubtractValue
			);
	}
	else
	{
		return FVector4(
			1.0f / ProjMatrix.M[2][2],
			-ProjMatrix.M[3][2] / ProjMatrix.M[2][2] + 1.0f,
			0.0f,
			1.0f
			);
	}
}

FSceneView::FSceneView(const FSceneViewInitOptions& InitOptions)
	: Family(InitOptions.ViewFamily)
	, State(InitOptions.SceneViewStateInterface)
	, ViewActor(InitOptions.ViewActor)
	, Drawer(InitOptions.ViewElementDrawer)
	, ViewRect(InitOptions.GetConstrainedViewRect())
	, UnscaledViewRect(InitOptions.GetConstrainedViewRect())
	, UnconstrainedViewRect(InitOptions.GetViewRect())
	, MaxShadowCascades(10)
	, WorldToMetersScale(InitOptions.WorldToMetersScale)
	, ProjectionMatrixUnadjustedForRHI(InitOptions.ProjectionMatrix)
	, BackgroundColor(InitOptions.BackgroundColor)
	, OverlayColor(InitOptions.OverlayColor)
	, ColorScale(InitOptions.ColorScale)
	, StereoPass(InitOptions.StereoPass)
	, bRenderFirstInstanceOnly(false)
	, DiffuseOverrideParameter(FVector4(0,0,0,1))
	, SpecularOverrideParameter(FVector4(0,0,0,1))
	, NormalOverrideParameter(FVector4(0,0,0,1))
	, RoughnessOverrideParameter(FVector2D(0,1))
	, HiddenPrimitives(InitOptions.HiddenPrimitives)
	, ShowOnlyPrimitives(InitOptions.ShowOnlyPrimitives)
	, LODDistanceFactor(InitOptions.LODDistanceFactor)
	, LODDistanceFactorSquared(InitOptions.LODDistanceFactor*InitOptions.LODDistanceFactor)
	, bCameraCut(InitOptions.bInCameraCut)
	, bOriginOffsetThisFrame(InitOptions.bOriginOffsetThisFrame)
	, CursorPos(InitOptions.CursorPos)
	, bIsGameView(false)
	, bIsViewInfo(false)
	, bIsSceneCapture(false)
	, bIsReflectionCapture(false)
	, bIsPlanarReflection(false)
	, bRenderSceneTwoSided(false)
	, bIsLocked(false)
	, bStaticSceneOnly(false)
	, bIsInstancedStereoEnabled(false)
	, bIsMultiViewEnabled(false)
	, bIsMobileMultiViewEnabled(false)
	, GlobalClippingPlane(FPlane(0, 0, 0, 0))
#if WITH_EDITOR
	, OverrideLODViewOrigin(InitOptions.OverrideLODViewOrigin)
	, bAllowTranslucentPrimitivesInHitProxy( true )
	, bHasSelectedComponents( false )
#endif
	, AntiAliasingMethod(AAM_None)
	, ForwardLightingResources(nullptr)
	, FeatureLevel(InitOptions.ViewFamily ? InitOptions.ViewFamily->GetFeatureLevel() : GMaxRHIFeatureLevel)
{
	check(UnscaledViewRect.Min.X >= 0);
	check(UnscaledViewRect.Min.Y >= 0);
	check(UnscaledViewRect.Width() > 0);
	check(UnscaledViewRect.Height() > 0);
	//check(InitOptions.ViewRotationMatrix.GetOrigin().IsNearlyZero());

	FVector ViewOrigin = InitOptions.ViewOrigin;
	FMatrix ViewRotationMatrix = InitOptions.ViewRotationMatrix;
	if( !ViewRotationMatrix.GetOrigin().IsNearlyZero( 0.0f ) )
	{
		ViewOrigin += ViewRotationMatrix.InverseTransformPosition( FVector::ZeroVector );
		ViewRotationMatrix = ViewRotationMatrix.RemoveTranslation();
	}

	ViewMatrices.ViewMatrix = FTranslationMatrix(-ViewOrigin) * ViewRotationMatrix;
	ViewMatrices.HMDViewMatrixNoRoll = InitOptions.ViewRotationMatrix;

	// Adjust the projection matrix for the current RHI.
	ViewMatrices.ProjMatrix = AdjustProjectionMatrixForRHI(ProjectionMatrixUnadjustedForRHI);

	// Compute the view projection matrix and its inverse.
	ViewProjectionMatrix = ViewMatrices.GetViewProjMatrix();

	FMatrix InvProjectionMatrix = ViewMatrices.GetInvProjMatrix();

	// For precision reasons the view matrix inverse is calculated independently.
	InvViewMatrix = ViewRotationMatrix.GetTransposed() * FTranslationMatrix(ViewOrigin);
	InvViewProjectionMatrix = InvProjectionMatrix * InvViewMatrix;

	bool bApplyPreViewTranslation = true;
	bool bViewOriginIsFudged = false;

	// Calculate the view origin from the view/projection matrices.
	if(IsPerspectiveProjection())
	{
		ViewMatrices.ViewOrigin = ViewOrigin;
	}
#if WITH_EDITOR
	else if (InitOptions.bUseFauxOrthoViewPos)
	{
		float DistanceToViewOrigin = WORLD_MAX;
		ViewMatrices.ViewOrigin = FVector4(InvViewMatrix.TransformVector(FVector(0,0,-1)).GetSafeNormal()*DistanceToViewOrigin,1) + ViewOrigin;
		bViewOriginIsFudged = true;
	}
#endif
	else
	{
		ViewMatrices.ViewOrigin = FVector4(InvViewMatrix.TransformVector(FVector(0,0,-1)).GetSafeNormal(),0);
		// to avoid issues with view dependent effect (e.g. Frensel)
		bApplyPreViewTranslation = false;
	}

	/** The view transform, starting from world-space points translated by -ViewOrigin. */
	FMatrix TranslatedViewMatrix = ViewRotationMatrix;
	FMatrix InvTranslatedViewMatrix = TranslatedViewMatrix.GetTransposed();

	// Translate world-space so its origin is at ViewOrigin for improved precision.
	// Note that this isn't exactly right for orthogonal projections (See the above special case), but we still use ViewOrigin
	// in that case so the same value may be used in shaders for both the world-space translation and the camera's world position.
	if(bApplyPreViewTranslation)
	{
		ViewMatrices.PreViewTranslation = -FVector(ViewOrigin);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		{
			// console variable override
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PreViewTranslation")); 
			int32 Value = CVar->GetValueOnGameThread();

			static FVector PreViewTranslationBackup;

			if(Value)
			{
				PreViewTranslationBackup = ViewMatrices.PreViewTranslation;
			}
			else
			{
				ViewMatrices.PreViewTranslation = PreViewTranslationBackup;
			}
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	}
	else
	{
		// If not applying PreViewTranslation then we need to use the view matrix directly.
		TranslatedViewMatrix = ViewMatrices.ViewMatrix;
		InvTranslatedViewMatrix = InvViewMatrix;
	}

	// When the view origin is fudged for faux ortho view position the translations don't cancel out.
	if (bViewOriginIsFudged)
	{
		TranslatedViewMatrix = FTranslationMatrix(-ViewMatrices.PreViewTranslation)
			* FTranslationMatrix(-ViewOrigin) * ViewRotationMatrix;
		InvTranslatedViewMatrix = TranslatedViewMatrix.Inverse();
	}
	
	// Compute a transform from view origin centered world-space to clip space.
	ViewMatrices.TranslatedViewMatrix = TranslatedViewMatrix;
	ViewMatrices.TranslatedViewProjectionMatrix = TranslatedViewMatrix * ViewMatrices.ProjMatrix;
	ViewMatrices.InvTranslatedViewProjectionMatrix = InvProjectionMatrix * InvTranslatedViewMatrix;

	// Compute screen scale factors.
	// Stereo renders at half horizontal resolution, but compute shadow resolution based on full resolution.
	const bool bStereo = StereoPass != eSSP_FULL;
	const float ScreenXScale = bStereo ? 2.0f : 1.0f;
	ViewMatrices.ProjectionScale.X = ScreenXScale * FMath::Abs(ViewMatrices.ProjMatrix.M[0][0]);
	ViewMatrices.ProjectionScale.Y = FMath::Abs(ViewMatrices.ProjMatrix.M[1][1]);
	ViewMatrices.ScreenScale = FMath::Max(
		ViewRect.Size().X * 0.5f * ViewMatrices.ProjectionScale.X,
		ViewRect.Size().Y * 0.5f * ViewMatrices.ProjectionScale.Y
		);
	
	ShadowViewMatrices = ViewMatrices;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		// console variable override
		int32 Value = CVarShadowFreezeCamera.GetValueOnAnyThread();

		static FViewMatrices Backup = ShadowViewMatrices;

		if(Value)
		{
			ShadowViewMatrices = Backup;
		}
		else
		{
			Backup = ShadowViewMatrices;
		}
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	if (InitOptions.OverrideFarClippingPlaneDistance > 0.0f)
	{
		const FPlane FarPlane(ViewMatrices.ViewOrigin + GetViewDirection() * InitOptions.OverrideFarClippingPlaneDistance, GetViewDirection());
		// Derive the view frustum from the view projection matrix, overriding the far plane
		GetViewFrustumBounds(ViewFrustum,ViewProjectionMatrix,FarPlane,true,false);
	}
	else
	{
		// Derive the view frustum from the view projection matrix.
		GetViewFrustumBounds(ViewFrustum,ViewProjectionMatrix,false);
	}

	// Derive the view's near clipping distance and plane.
	// The GetFrustumFarPlane() is the near plane because of reverse Z projection.
	static_assert((int32)ERHIZBuffer::IsInverted != 0, "Fix Near Clip distance!");
	bHasNearClippingPlane = ViewProjectionMatrix.GetFrustumFarPlane(NearClippingPlane);
	if(ViewMatrices.ProjMatrix.M[2][3] > DELTA)
	{
		// Infinite projection with reversed Z.
		NearClippingDistance = ViewMatrices.ProjMatrix.M[3][2];
	}
	else
	{
		// Ortho projection with reversed Z.
		NearClippingDistance = (1.0f - ViewMatrices.ProjMatrix.M[3][2]) / ViewMatrices.ProjMatrix.M[2][2];
	}

	// Determine whether the view should reverse the cull mode due to a negative determinant.  Only do this for a valid scene
	bReverseCulling = (Family && Family->Scene) ? FMath::IsNegativeFloat(ViewMatrices.ViewMatrix.Determinant()) : false;

	// OpenGL Gamma space output in GLSL flips Y when rendering directly to the back buffer (so not needed on PC, as we never render directly into the back buffer)
	auto ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];
	bool bUsingMobileRenderer = FSceneInterface::GetShadingPath(FeatureLevel) == EShadingPath::Mobile;
	bool bPlatformRequiresReverseCulling = (IsOpenGLPlatform(ShaderPlatform) && bUsingMobileRenderer && !IsPCPlatform(ShaderPlatform) && !IsVulkanMobilePlatform(ShaderPlatform));
	static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	check(MobileHDRCvar);
	bReverseCulling = (bPlatformRequiresReverseCulling && MobileHDRCvar->GetValueOnAnyThread() == 0) ? !bReverseCulling : bReverseCulling;

	// Setup transformation constants to be used by the graphics hardware to transform device normalized depth samples
	// into world oriented z.
	InvDeviceZToWorldZTransform = CreateInvDeviceZToWorldZTransform(ProjectionMatrixUnadjustedForRHI);

	static TConsoleVariableData<int32>* SortPolicyCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.TranslucentSortPolicy"));
	TranslucentSortPolicy = static_cast<ETranslucentSortPolicy::Type>(SortPolicyCvar->GetValueOnAnyThread());

	TranslucentSortAxis = GetDefault<URendererSettings>()->TranslucentSortAxis;

	// As the world is only accessible from the game thread, bIsGameView should be explicitly
	// set on any other thread.
	if(IsInGameThread())
	{
		bIsGameView = (Family && Family->Scene && Family->Scene->GetWorld() ) ? Family->Scene->GetWorld()->IsGameWorld() : false;
	}

	bUseFieldOfViewForLOD = InitOptions.bUseFieldOfViewForLOD;
	DrawDynamicFlags = EDrawDynamicFlags::None;
	bAllowTemporalJitter = true;
	TemporalJitterPixelsX = 0.0f;
	TemporalJitterPixelsY = 0.0f;

#if WITH_EDITOR
	bUsePixelInspector = false;

	EditorViewBitflag = InitOptions.EditorViewBitflag;

	SelectionOutlineColor = GEngine->GetSelectionOutlineColor();
#endif

	// Query instanced stereo and multi-view state
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.InstancedStereo"));
	bIsInstancedStereoEnabled = (ShaderPlatform == EShaderPlatform::SP_PCD3D_SM5 || ShaderPlatform == EShaderPlatform::SP_PS4) ? (CVar ? (CVar->GetValueOnAnyThread() != false) : false) : false;

	static const auto MultiViewCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MultiView"));
	bIsMultiViewEnabled = ShaderPlatform == EShaderPlatform::SP_PS4 && (MultiViewCVar && MultiViewCVar->GetValueOnAnyThread() != 0);

	static const auto MobileMultiViewCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MobileMultiView"));
	bIsMobileMultiViewEnabled = GSupportsMobileMultiView && (MobileMultiViewCVar && MobileMultiViewCVar->GetValueOnAnyThread() != 0);

	SetupAntiAliasingMethod();
}

void FSceneView::SetupAntiAliasingMethod()
{
	{
		int32 Value = CVarDefaultAntiAliasing.GetValueOnAnyThread();
		if (Value >= 0 && Value < AAM_MAX)
		{
			AntiAliasingMethod = (EAntiAliasingMethod)Value;
		}
	}

	static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
	if (FeatureLevel <= ERHIFeatureLevel::ES3_1 && CVarMobileMSAA ? CVarMobileMSAA->GetValueOnAnyThread() > 1 : false)
	{
		//@todo Ronin check what we support under MSAA

		// Turn off various features which won't work with mobile MSAA.
		//FinalPostProcessSettings.DepthOfFieldScale = 0.0f;
		AntiAliasingMethod = AAM_None;
	}

	if (Family)
	{
		static IConsoleVariable* CVarMSAACount = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MSAACount"));

		if (AntiAliasingMethod == AAM_MSAA && IsForwardShadingEnabled(FeatureLevel) && CVarMSAACount->GetInt() <= 1)
		{
			// Fallback to temporal AA so we can easily toggle methods with r.MSAACount
			AntiAliasingMethod = AAM_TemporalAA;
		}

		static const auto PostProcessAAQualityCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessAAQuality"));
		static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
		static auto* MobileMSAACvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
		static uint32 MSAAValue = GShaderPlatformForFeatureLevel[FeatureLevel] == SP_OPENGL_ES2_IOS ? 1 : MobileMSAACvar->GetValueOnAnyThread();

		int32 Quality = FMath::Clamp(PostProcessAAQualityCVar->GetValueOnAnyThread(), 0, 6);
		const bool bWillApplyTemporalAA = Family->EngineShowFlags.PostProcessing || bIsPlanarReflection;

		if (!bWillApplyTemporalAA || !Family->EngineShowFlags.AntiAliasing || Quality <= 0
			// Disable antialiasing in GammaLDR mode to avoid jittering.
			|| (FeatureLevel == ERHIFeatureLevel::ES2 && MobileHDRCvar->GetValueOnAnyThread() == 0)
			|| (FeatureLevel <= ERHIFeatureLevel::ES3_1 && (MSAAValue > 1))
			|| Family->EngineShowFlags.VisualizeBloom
			|| Family->EngineShowFlags.VisualizeDOF)
		{
			AntiAliasingMethod = AAM_None;
		}

		if (AntiAliasingMethod == AAM_TemporalAA)
		{
			if (!Family->EngineShowFlags.TemporalAA || !Family->bRealtimeUpdate || Quality < 3)
			{
				AntiAliasingMethod = AAM_FXAA;
			}
		}
	}
}

static TAutoConsoleVariable<int32> CVarCompensateForFOV(
	TEXT("lod.CompensateForFOV"),
	1,
	TEXT("When not 0 account for FOV in LOD calculations."));

float FSceneView::GetLODDistanceFactor() const
{
	bool bCompensateForFOV = bUseFieldOfViewForLOD && CVarCompensateForFOV.GetValueOnAnyThread() != 0;
	float ScreenScaleX = bCompensateForFOV ? ViewMatrices.ProjMatrix.M[0][0] : 1.0f;
	float ScreenScaleY = bCompensateForFOV ? ViewMatrices.ProjMatrix.M[1][1] : (static_cast<float>(ViewRect.Width()) / ViewRect.Height());

	const float ScreenMultiple = FMath::Max(ViewRect.Width() / 2.0f * ScreenScaleX,
		ViewRect.Height() / 2.0f * ScreenScaleY);
	float Fac = PI * ScreenMultiple * ScreenMultiple / ViewRect.Area();
	return Fac;
}

float FSceneView::GetTemporalLODDistanceFactor(int32 Index, bool bUseLaggedLODTransition) const
{
	if (bUseLaggedLODTransition && State)
	{
		const FTemporalLODState& LODState = State->GetTemporalLODState();
		if (LODState.TemporalLODLag != 0.0f)
		{
			return LODState.TemporalDistanceFactor[Index];
		}
	}
	return GetLODDistanceFactor();
}


FVector FSceneView::GetTemporalLODOrigin(int32 Index, bool bUseLaggedLODTransition) const
{
	if (bUseLaggedLODTransition && State)
	{
		const FTemporalLODState& LODState = State->GetTemporalLODState();
		if (LODState.TemporalLODLag != 0.0f)
		{
			return LODState.TemporalLODViewOrigin[Index];
		}
	}
	return ViewMatrices.ViewOrigin;
}

float FSceneView::GetTemporalLODTransition() const
{
	if (State)
	{
		return State->GetTemporalLODTransition();
	}
	return 0.0f;
}

uint32 FSceneView::GetViewKey() const
{
	if (State)
	{
		return State->GetViewKey();
	}
	return 0;
}

uint32 FSceneView::GetOcclusionFrameCounter() const
{
	if (State)
	{
		return State->GetOcclusionFrameCounter();
	}
	return MAX_uint32;
}


void FSceneView::UpdateViewMatrix()
{
	FVector StereoViewLocation = ViewLocation;
	if (GEngine->StereoRenderingDevice.IsValid() && StereoPass != eSSP_FULL)
	{
		GEngine->StereoRenderingDevice->CalculateStereoViewOffset(StereoPass, ViewRotation, WorldToMetersScale, StereoViewLocation);
		ViewLocation = StereoViewLocation;
	}

	ViewMatrices.ViewOrigin = StereoViewLocation;

	FMatrix ViewPlanesMatrix = FMatrix(
		FPlane(0,	0,	1,	0),
		FPlane(1,	0,	0,	0),
		FPlane(0,	1,	0,	0),
		FPlane(0,	0,	0,	1));

	ViewMatrices.ViewMatrix = FTranslationMatrix(-StereoViewLocation);
	ViewMatrices.ViewMatrix = ViewMatrices.ViewMatrix * FInverseRotationMatrix(ViewRotation);
	ViewMatrices.ViewMatrix = ViewMatrices.ViewMatrix * ViewPlanesMatrix;

	// Duplicate HMD rotation matrix with roll removed
	FRotator HMDViewRotation = ViewRotation;
	HMDViewRotation.Roll = 0.f;
	ViewMatrices.HMDViewMatrixNoRoll = FInverseRotationMatrix(HMDViewRotation) * ViewPlanesMatrix;
 
	ViewMatrices.PreViewTranslation = -ViewMatrices.ViewOrigin;
	ViewMatrices.TranslatedViewMatrix = FTranslationMatrix(-ViewMatrices.PreViewTranslation) * ViewMatrices.ViewMatrix;
	
	// Compute a transform from view origin centered world-space to clip space.
	ViewMatrices.TranslatedViewProjectionMatrix = ViewMatrices.TranslatedViewMatrix * ViewMatrices.ProjMatrix;
	ViewMatrices.InvTranslatedViewProjectionMatrix = ViewMatrices.TranslatedViewProjectionMatrix.Inverse();

	ViewProjectionMatrix = ViewMatrices.GetViewProjMatrix();
	InvViewMatrix = ViewMatrices.GetInvViewMatrix();
	InvViewProjectionMatrix = ViewMatrices.GetInvProjMatrix() * InvViewMatrix;

	// Derive the view frustum from the view projection matrix.
	GetViewFrustumBounds(ViewFrustum, ViewProjectionMatrix, false);

	// We need to keep ShadowViewMatrices in sync.
	ShadowViewMatrices = ViewMatrices;
}
void FSceneView::UpdatePlanarReflectionViewMatrix(const FSceneView& SourceView, const FMirrorMatrix& MirrorMatrix)
{
	// This is a subset of the FSceneView ctor that recomputes the transforms changed by late updating the parent camera (in UpdateViewMatrix)
	const FMatrix ViewMatrix(MirrorMatrix * SourceView.ViewMatrices.ViewMatrix);
	ViewMatrices.HMDViewMatrixNoRoll = ViewMatrix.RemoveTranslation();
	
	ViewMatrices.ViewOrigin = ViewMatrix.InverseTransformPosition(FVector::ZeroVector);
	ViewMatrices.PreViewTranslation = -ViewMatrices.ViewOrigin;

	ViewMatrices.ViewMatrix = FTranslationMatrix(-ViewMatrices.ViewOrigin) * ViewMatrices.HMDViewMatrixNoRoll;

	ViewProjectionMatrix = ViewMatrices.GetViewProjMatrix();
	const FMatrix InvProjectionMatrix = ViewMatrices.GetInvProjMatrix();

	InvViewMatrix = ViewMatrices.HMDViewMatrixNoRoll.GetTransposed() * FTranslationMatrix(ViewMatrices.ViewOrigin);
	InvViewProjectionMatrix = InvProjectionMatrix * InvViewMatrix;

	ViewMatrices.TranslatedViewMatrix = ViewMatrices.HMDViewMatrixNoRoll;
	const FMatrix InvTranslatedViewMatrix = ViewMatrices.TranslatedViewMatrix.GetTransposed();

	ViewMatrices.TranslatedViewProjectionMatrix = ViewMatrices.TranslatedViewMatrix * ViewMatrices.ProjMatrix;
	ViewMatrices.InvTranslatedViewProjectionMatrix = InvProjectionMatrix * InvTranslatedViewMatrix;

	// Update bounds
	GetViewFrustumBounds(ViewFrustum, ViewProjectionMatrix, false);

	// We need to keep ShadowViewMatrices in sync.
	ShadowViewMatrices = ViewMatrices;
}

void FSceneView::SetScaledViewRect(FIntRect InScaledViewRect)
{
	check(InScaledViewRect.Min.X >= 0);
	check(InScaledViewRect.Min.Y >= 0);
	check(InScaledViewRect.Width() > 0);
	check(InScaledViewRect.Height() > 0);

	check(ViewRect == UnscaledViewRect);

	ViewRect = InScaledViewRect;
}

FVector4 FSceneView::WorldToScreen(const FVector& WorldPoint) const
{
	return ViewProjectionMatrix.TransformFVector4(FVector4(WorldPoint,1));
}

FVector FSceneView::ScreenToWorld(const FVector4& ScreenPoint) const
{
	return InvViewProjectionMatrix.TransformFVector4(ScreenPoint);
}

bool FSceneView::ScreenToPixel(const FVector4& ScreenPoint,FVector2D& OutPixelLocation) const
{
	if(ScreenPoint.W > 0.0f)
	{
		float InvW = 1.0f / ScreenPoint.W;
		float Y = (GProjectionSignY > 0.0f) ? ScreenPoint.Y : 1.0f - ScreenPoint.Y;
		OutPixelLocation = FVector2D(
			UnscaledViewRect.Min.X + (0.5f + ScreenPoint.X * 0.5f * InvW) * UnscaledViewRect.Width(),
			UnscaledViewRect.Min.Y + (0.5f - Y * 0.5f * InvW) * UnscaledViewRect.Height()
			);
		return true;
	}
	else
	{
		return false;
	}
}

FVector4 FSceneView::PixelToScreen(float InX,float InY,float Z) const
{
	if (GProjectionSignY > 0.0f)
	{
		return FVector4(
			-1.0f + InX / UnscaledViewRect.Width() * +2.0f,
			+1.0f + InY / UnscaledViewRect.Height() * -2.0f,
			Z,
			1
			);
	}
	else
	{
		return FVector4(
			-1.0f + InX / UnscaledViewRect.Width() * +2.0f,
			1.0f - (+1.0f + InY / UnscaledViewRect.Height() * -2.0f),
			Z,
			1
			);
	}
}

/** Transforms a point from the view's world-space into pixel coordinates relative to the view's X,Y. */
bool FSceneView::WorldToPixel(const FVector& WorldPoint,FVector2D& OutPixelLocation) const
{
	const FVector4 ScreenPoint = WorldToScreen(WorldPoint);
	return ScreenToPixel(ScreenPoint, OutPixelLocation);
}

/** Transforms a point from pixel coordinates relative to the view's X,Y (left, top) into the view's world-space. */
FVector4 FSceneView::PixelToWorld(float X,float Y,float Z) const
{
	const FVector4 ScreenPoint = PixelToScreen(X, Y, Z);
	return ScreenToWorld(ScreenPoint);
}

/**  
 * Transforms a point from the view's world-space into the view's screen-space.  
 * Divides the resulting X, Y, Z by W before returning. 
 */
FPlane FSceneView::Project(const FVector& WorldPoint) const
{
	FPlane Result = WorldToScreen(WorldPoint);

	if (Result.W == 0)
	{
		Result.W = KINDA_SMALL_NUMBER;
	}

	const float RHW = 1.0f / Result.W;

	return FPlane(Result.X * RHW,Result.Y * RHW,Result.Z * RHW,Result.W);
}

/** 
 * Transforms a point from the view's screen-space into world coordinates
 * multiplies X, Y, Z by W before transforming. 
 */
FVector FSceneView::Deproject(const FPlane& ScreenPoint) const
{
	return InvViewProjectionMatrix.TransformFVector4(FPlane(ScreenPoint.X * ScreenPoint.W,ScreenPoint.Y * ScreenPoint.W,ScreenPoint.Z * ScreenPoint.W,ScreenPoint.W));
}

void FSceneView::DeprojectFVector2D(const FVector2D& ScreenPos, FVector& out_WorldOrigin, FVector& out_WorldDirection) const
{
	const FMatrix InvViewProjMatrix = ViewMatrices.GetInvViewProjMatrix();
	DeprojectScreenToWorld(ScreenPos, UnscaledViewRect, InvViewProjectionMatrix, out_WorldOrigin, out_WorldDirection);
}

void FSceneView::DeprojectScreenToWorld(const FVector2D& ScreenPos, const FIntRect& ViewRect, const FMatrix& InvViewMatrix, const FMatrix& InvProjectionMatrix, FVector& out_WorldOrigin, FVector& out_WorldDirection)
{
	int32 PixelX = FMath::TruncToInt(ScreenPos.X);
	int32 PixelY = FMath::TruncToInt(ScreenPos.Y);

	// Get the eye position and direction of the mouse cursor in two stages (inverse transform projection, then inverse transform view).
	// This avoids the numerical instability that occurs when a view matrix with large translation is composed with a projection matrix

	// Get the pixel coordinates into 0..1 normalized coordinates within the constrained view rectangle
	const float NormalizedX = (PixelX - ViewRect.Min.X) / ((float)ViewRect.Width());
	const float NormalizedY = (PixelY - ViewRect.Min.Y) / ((float)ViewRect.Height());

	// Get the pixel coordinates into -1..1 projection space
	const float ScreenSpaceX = (NormalizedX - 0.5f) * 2.0f;
	const float ScreenSpaceY = ((1.0f - NormalizedY) - 0.5f) * 2.0f;

	// The start of the raytrace is defined to be at mousex,mousey,1 in projection space (z=1 is near, z=0 is far - this gives us better precision)
	// To get the direction of the raytrace we need to use any z between the near and the far plane, so let's use (mousex, mousey, 0.5)
	const FVector4 RayStartProjectionSpace = FVector4(ScreenSpaceX, ScreenSpaceY, 1.0f, 1.0f);
	const FVector4 RayEndProjectionSpace = FVector4(ScreenSpaceX, ScreenSpaceY, 0.5f, 1.0f);

	// Projection (changing the W coordinate) is not handled by the FMatrix transforms that work with vectors, so multiplications
	// by the projection matrix should use homogeneous coordinates (i.e. FPlane).
	const FVector4 HGRayStartViewSpace = InvProjectionMatrix.TransformFVector4(RayStartProjectionSpace);
	const FVector4 HGRayEndViewSpace = InvProjectionMatrix.TransformFVector4(RayEndProjectionSpace);
	FVector RayStartViewSpace(HGRayStartViewSpace.X, HGRayStartViewSpace.Y, HGRayStartViewSpace.Z);
	FVector RayEndViewSpace(HGRayEndViewSpace.X,   HGRayEndViewSpace.Y,   HGRayEndViewSpace.Z);
	// divide vectors by W to undo any projection and get the 3-space coordinate 
	if (HGRayStartViewSpace.W != 0.0f)
	{
		RayStartViewSpace /= HGRayStartViewSpace.W;
	}
	if (HGRayEndViewSpace.W != 0.0f)
	{
		RayEndViewSpace /= HGRayEndViewSpace.W;
	}
	FVector RayDirViewSpace = RayEndViewSpace - RayStartViewSpace;
	RayDirViewSpace = RayDirViewSpace.GetSafeNormal();

	// The view transform does not have projection, so we can use the standard functions that deal with vectors and normals (normals
	// are vectors that do not use the translational part of a rotation/translation)
	const FVector RayStartWorldSpace = InvViewMatrix.TransformPosition(RayStartViewSpace);
	const FVector RayDirWorldSpace = InvViewMatrix.TransformVector(RayDirViewSpace);

	// Finally, store the results in the hitcheck inputs.  The start position is the eye, and the end position
	// is the eye plus a long distance in the direction the mouse is pointing.
	out_WorldOrigin = RayStartWorldSpace;
	out_WorldDirection = RayDirWorldSpace.GetSafeNormal();
}

void FSceneView::DeprojectScreenToWorld(const FVector2D& ScreenPos, const FIntRect& ViewRect, const FMatrix& InvViewProjMatrix, FVector& out_WorldOrigin, FVector& out_WorldDirection)
{
	float PixelX = FMath::TruncToFloat(ScreenPos.X);
	float PixelY = FMath::TruncToFloat(ScreenPos.Y);

	// Get the eye position and direction of the mouse cursor in two stages (inverse transform projection, then inverse transform view).
	// This avoids the numerical instability that occurs when a view matrix with large translation is composed with a projection matrix

	// Get the pixel coordinates into 0..1 normalized coordinates within the constrained view rectangle
	const float NormalizedX = (PixelX - ViewRect.Min.X) / ((float)ViewRect.Width());
	const float NormalizedY = (PixelY - ViewRect.Min.Y) / ((float)ViewRect.Height());

	// Get the pixel coordinates into -1..1 projection space
	const float ScreenSpaceX = (NormalizedX - 0.5f) * 2.0f;
	const float ScreenSpaceY = ((1.0f - NormalizedY) - 0.5f) * 2.0f;

	// The start of the raytrace is defined to be at mousex,mousey,1 in projection space (z=1 is near, z=0 is far - this gives us better precision)
	// To get the direction of the raytrace we need to use any z between the near and the far plane, so let's use (mousex, mousey, 0.5)
	const FVector4 RayStartProjectionSpace = FVector4(ScreenSpaceX, ScreenSpaceY, 1.0f, 1.0f);
	const FVector4 RayEndProjectionSpace = FVector4(ScreenSpaceX, ScreenSpaceY, 0.5f, 1.0f);

	// Projection (changing the W coordinate) is not handled by the FMatrix transforms that work with vectors, so multiplications
	// by the projection matrix should use homogeneous coordinates (i.e. FPlane).
	const FVector4 HGRayStartWorldSpace = InvViewProjMatrix.TransformFVector4(RayStartProjectionSpace);
	const FVector4 HGRayEndWorldSpace = InvViewProjMatrix.TransformFVector4(RayEndProjectionSpace);
	FVector RayStartWorldSpace(HGRayStartWorldSpace.X, HGRayStartWorldSpace.Y, HGRayStartWorldSpace.Z);
	FVector RayEndWorldSpace(HGRayEndWorldSpace.X, HGRayEndWorldSpace.Y, HGRayEndWorldSpace.Z);
	// divide vectors by W to undo any projection and get the 3-space coordinate 
	if (HGRayStartWorldSpace.W != 0.0f)
	{
		RayStartWorldSpace /= HGRayStartWorldSpace.W;
	}
	if (HGRayEndWorldSpace.W != 0.0f)
	{
		RayEndWorldSpace /= HGRayEndWorldSpace.W;
	}
	const FVector RayDirWorldSpace = (RayEndWorldSpace - RayStartWorldSpace).GetSafeNormal();

	// Finally, store the results in the outputs
	out_WorldOrigin = RayStartWorldSpace;
	out_WorldDirection = RayDirWorldSpace;
}

bool FSceneView::ProjectWorldToScreen(const FVector& WorldPosition, const FIntRect& ViewRect, const FMatrix& ViewProjectionMatrix, FVector2D& out_ScreenPos)
{
	FPlane Result = ViewProjectionMatrix.TransformFVector4(FVector4(WorldPosition, 1.f));
	if ( Result.W > 0.0f )
	{
		// the result of this will be x and y coords in -1..1 projection space
		const float RHW = 1.0f / Result.W;
		FPlane PosInScreenSpace = FPlane(Result.X * RHW, Result.Y * RHW, Result.Z * RHW, Result.W);

		// Move from projection space to normalized 0..1 UI space
		const float NormalizedX = ( PosInScreenSpace.X / 2.f ) + 0.5f;
		const float NormalizedY = 1.f - ( PosInScreenSpace.Y / 2.f ) - 0.5f;

		FVector2D RayStartViewRectSpace(
			( NormalizedX * (float)ViewRect.Width() ),
			( NormalizedY * (float)ViewRect.Height() )
			);

		out_ScreenPos = RayStartViewRectSpace;

		return true;
	}
	
	return false;
}


#define LERP_PP(NAME) if(Src.bOverride_ ## NAME)	Dest . NAME = FMath::Lerp(Dest . NAME, Src . NAME, Weight);
#define IF_PP(NAME) if(Src.bOverride_ ## NAME && Src . NAME)

// @param Weight 0..1
void FSceneView::OverridePostProcessSettings(const FPostProcessSettings& Src, float Weight)
{
	SCOPE_CYCLE_COUNTER(STAT_OverridePostProcessSettings);

	if(Weight <= 0.0f)
	{
		// no need to blend anything
		return;
	}

	if(Weight > 1.0f)
	{
		Weight = 1.0f;
	}

	{
		FFinalPostProcessSettings& Dest = FinalPostProcessSettings;

		// The following code needs to be adjusted when settings in FPostProcessSettings change.
		LERP_PP(WhiteTemp);
		LERP_PP(WhiteTint);

		LERP_PP(ColorSaturation);
		LERP_PP(ColorContrast);
		LERP_PP(ColorGamma);
		LERP_PP(ColorGain);
		LERP_PP(ColorOffset);

		LERP_PP(ColorSaturationShadows);
		LERP_PP(ColorContrastShadows);
		LERP_PP(ColorGammaShadows);
		LERP_PP(ColorGainShadows);
		LERP_PP(ColorOffsetShadows);

		LERP_PP(ColorSaturationMidtones);
		LERP_PP(ColorContrastMidtones);
		LERP_PP(ColorGammaMidtones);
		LERP_PP(ColorGainMidtones);
		LERP_PP(ColorOffsetMidtones);

		LERP_PP(ColorSaturationHighlights);
		LERP_PP(ColorContrastHighlights);
		LERP_PP(ColorGammaHighlights);
		LERP_PP(ColorGainHighlights);
		LERP_PP(ColorOffsetHighlights);

		LERP_PP(ColorCorrectionShadowsMax);
		LERP_PP(ColorCorrectionHighlightsMin);

		LERP_PP(FilmWhitePoint);
		LERP_PP(FilmSaturation);
		LERP_PP(FilmChannelMixerRed);
		LERP_PP(FilmChannelMixerGreen);
		LERP_PP(FilmChannelMixerBlue);
		LERP_PP(FilmContrast);
		LERP_PP(FilmDynamicRange);
		LERP_PP(FilmHealAmount);
		LERP_PP(FilmToeAmount);
		LERP_PP(FilmShadowTint);
		LERP_PP(FilmShadowTintBlend);
		LERP_PP(FilmShadowTintAmount);

		LERP_PP(FilmSlope);
		LERP_PP(FilmToe);
		LERP_PP(FilmShoulder);
		LERP_PP(FilmBlackClip);
		LERP_PP(FilmWhiteClip);

		LERP_PP(SceneColorTint);
		LERP_PP(SceneFringeIntensity);
		LERP_PP(BloomIntensity);
		LERP_PP(BloomThreshold);
		LERP_PP(Bloom1Tint);
		LERP_PP(BloomSizeScale);
		LERP_PP(Bloom1Size);
		LERP_PP(Bloom2Tint);
		LERP_PP(Bloom2Size);
		LERP_PP(Bloom3Tint);
		LERP_PP(Bloom3Size);
		LERP_PP(Bloom4Tint);
		LERP_PP(Bloom4Size);
		LERP_PP(Bloom5Tint);
		LERP_PP(Bloom5Size);
		LERP_PP(Bloom6Tint);
		LERP_PP(Bloom6Size);
		LERP_PP(BloomDirtMaskIntensity);
		LERP_PP(BloomDirtMaskTint);
		LERP_PP(AmbientCubemapIntensity);
		LERP_PP(AmbientCubemapTint);
		LERP_PP(AutoExposureLowPercent);
		LERP_PP(AutoExposureHighPercent);
		LERP_PP(AutoExposureMinBrightness);
		LERP_PP(AutoExposureMaxBrightness);
		LERP_PP(AutoExposureSpeedUp);
		LERP_PP(AutoExposureSpeedDown);
		LERP_PP(AutoExposureBias);
		LERP_PP(HistogramLogMin);
		LERP_PP(HistogramLogMax);
		LERP_PP(LensFlareIntensity);
		LERP_PP(LensFlareTint);
		LERP_PP(LensFlareBokehSize);
		LERP_PP(LensFlareThreshold);
		LERP_PP(VignetteIntensity);
		LERP_PP(GrainIntensity);
		LERP_PP(GrainJitter);
		LERP_PP(AmbientOcclusionIntensity);
		LERP_PP(AmbientOcclusionStaticFraction);
		LERP_PP(AmbientOcclusionRadius);
		LERP_PP(AmbientOcclusionFadeDistance);
		LERP_PP(AmbientOcclusionFadeRadius);
		LERP_PP(AmbientOcclusionDistance_DEPRECATED);
		LERP_PP(AmbientOcclusionPower);
		LERP_PP(AmbientOcclusionBias);
		LERP_PP(AmbientOcclusionQuality);
		LERP_PP(AmbientOcclusionMipBlend);
		LERP_PP(AmbientOcclusionMipScale);
		LERP_PP(AmbientOcclusionMipThreshold);
		LERP_PP(IndirectLightingColor);
		LERP_PP(IndirectLightingIntensity);
		LERP_PP(DepthOfFieldFocalDistance);
		LERP_PP(DepthOfFieldFstop);
		LERP_PP(DepthOfFieldSensorWidth);
		LERP_PP(DepthOfFieldDepthBlurRadius);
		LERP_PP(DepthOfFieldDepthBlurAmount);
		LERP_PP(DepthOfFieldFocalRegion);
		LERP_PP(DepthOfFieldNearTransitionRegion);
		LERP_PP(DepthOfFieldFarTransitionRegion);
		LERP_PP(DepthOfFieldScale);
		LERP_PP(DepthOfFieldMaxBokehSize);
		LERP_PP(DepthOfFieldNearBlurSize);
		LERP_PP(DepthOfFieldFarBlurSize);
		LERP_PP(DepthOfFieldOcclusion);
		LERP_PP(DepthOfFieldColorThreshold);
		LERP_PP(DepthOfFieldSizeThreshold);
		LERP_PP(DepthOfFieldSkyFocusDistance);
		LERP_PP(DepthOfFieldVignetteSize);
		LERP_PP(MotionBlurAmount);
		LERP_PP(MotionBlurMax);
		LERP_PP(MotionBlurPerObjectSize);
		LERP_PP(ScreenPercentage);
		LERP_PP(ScreenSpaceReflectionQuality);
		LERP_PP(ScreenSpaceReflectionIntensity);
		LERP_PP(ScreenSpaceReflectionMaxRoughness);

		// cubemaps are getting blended additively - in contrast to other properties, maybe we should make that consistent
		if (Src.AmbientCubemap && Src.bOverride_AmbientCubemapIntensity)
		{
			FFinalPostProcessSettings::FCubemapEntry Entry;

			Entry.AmbientCubemapTintMulScaleValue = FLinearColor(1, 1, 1, 1) * Src.AmbientCubemapIntensity;

			if (Src.bOverride_AmbientCubemapTint)
			{
				Entry.AmbientCubemapTintMulScaleValue *= Src.AmbientCubemapTint;
			}

			Entry.AmbientCubemap = Src.AmbientCubemap;
			Dest.UpdateEntry(Entry, Weight);
		}

		IF_PP(ColorGradingLUT)
		{
			float ColorGradingIntensity = FMath::Clamp(Src.ColorGradingIntensity, 0.0f, 1.0f);
			Dest.LerpTo(Src.ColorGradingLUT, ColorGradingIntensity * Weight);
		}

		// actual texture cannot be blended but the intensity can be blended
		IF_PP(BloomDirtMask)
		{
			Dest.BloomDirtMask = Src.BloomDirtMask;
		}

		// actual texture cannot be blended but the intensity can be blended
		IF_PP(DepthOfFieldBokehShape)
		{
			Dest.DepthOfFieldBokehShape = Src.DepthOfFieldBokehShape;
		}

		// actual texture cannot be blended but the intensity can be blended
		IF_PP(LensFlareBokehShape)
		{
			Dest.LensFlareBokehShape = Src.LensFlareBokehShape;
		}

		if (Src.bOverride_LensFlareTints)
		{
			for (uint32 i = 0; i < 8; ++i)
			{
				Dest.LensFlareTints[i] = FMath::Lerp(Dest.LensFlareTints[i], Src.LensFlareTints[i], Weight);
			}
		}

		if (Src.bOverride_DepthOfFieldMethod)
		{
			Dest.DepthOfFieldMethod = Src.DepthOfFieldMethod;
		}

		if (Src.bOverride_MobileHQGaussian)
		{
			Dest.bMobileHQGaussian = Src.bMobileHQGaussian;
		}	

		if (Src.bOverride_AutoExposureMethod)
		{
			Dest.AutoExposureMethod = Src.AutoExposureMethod;
		}

		if (Src.bOverride_AmbientOcclusionRadiusInWS)
		{
			Dest.AmbientOcclusionRadiusInWS = Src.AmbientOcclusionRadiusInWS;
		}
	}
	
	// will be deprecated soon, use the new asset LightPropagationVolumeBlendable instead
	{
		FLightPropagationVolumeSettings& Dest = FinalPostProcessSettings.BlendableManager.GetSingleFinalData<FLightPropagationVolumeSettings>();

		LERP_PP(LPVIntensity);
		LERP_PP(LPVSecondaryOcclusionIntensity);
		LERP_PP(LPVSecondaryBounceIntensity);
		LERP_PP(LPVVplInjectionBias);
		LERP_PP(LPVGeometryVolumeBias);
		LERP_PP(LPVEmissiveInjectionIntensity);
		LERP_PP(LPVDirectionalOcclusionIntensity);
		LERP_PP(LPVDirectionalOcclusionRadius);
		LERP_PP(LPVDiffuseOcclusionExponent);
		LERP_PP(LPVSpecularOcclusionExponent);
		LERP_PP(LPVDiffuseOcclusionIntensity);
		LERP_PP(LPVSpecularOcclusionIntensity);

		if (Src.bOverride_LPVSize)
		{
			Dest.LPVSize = Src.LPVSize;
		}
	}

	// Blendable objects
	{
		uint32 Count = Src.WeightedBlendables.Array.Num();

		for(uint32 i = 0; i < Count; ++i)
		{
			UObject* Object = Src.WeightedBlendables.Array[i].Object;

			if(!Object)
			{
				continue;
			}

			IBlendableInterface* BlendableInterface = Cast<IBlendableInterface>(Object);
			
			if(!BlendableInterface)
			{
				continue;
			}

			float LocalWeight = FMath::Min(1.0f, Src.WeightedBlendables.Array[i].Weight) * Weight;

			if(LocalWeight > 0.0f)
			{
				BlendableInterface->OverrideBlendableSettings(*this, LocalWeight);
			}
		}
	}
}

/** Dummy class needed to support Cast<IBlendableInterface>(Object) */
UBlendableInterface::UBlendableInterface(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) 
{
}

void DoPostProcessVolume(IInterface_PostProcessVolume* Volume, FVector ViewLocation, FSceneView* SceneView)
{
	const FPostProcessVolumeProperties VolumeProperties = Volume->GetProperties();
	if (!VolumeProperties.bIsEnabled)
	{
		return;
	}

	float DistanceToPoint = 0.0f;
	float LocalWeight = FMath::Clamp(VolumeProperties.BlendWeight, 0.0f, 1.0f);

	if (!VolumeProperties.bIsUnbound)
	{
		float SquaredBlendRadius = VolumeProperties.BlendRadius * VolumeProperties.BlendRadius;
		Volume->EncompassesPoint(ViewLocation, 0.0f, &DistanceToPoint);

		if (DistanceToPoint >= 0)
		{
			if (DistanceToPoint > VolumeProperties.BlendRadius)
			{
				// outside
				LocalWeight = 0.0f;
			}
			else
			{
				// to avoid div by 0
				if (VolumeProperties.BlendRadius >= 1.0f)
				{
					LocalWeight *= 1.0f - DistanceToPoint / VolumeProperties.BlendRadius;

					check(LocalWeight >= 0 && LocalWeight <= 1.0f);
				}
			}
		}
		else
		{
			LocalWeight = 0;
		}
	}

	if (LocalWeight > 0)
	{
		SceneView->OverridePostProcessSettings(*VolumeProperties.Settings, LocalWeight);
	}
}

void FSceneView::StartFinalPostprocessSettings(FVector InViewLocation)
{
	SCOPE_CYCLE_COUNTER(STAT_StartFinalPostprocessSettings);

	check(IsInGameThread());

	// The final settings for the current viewer position (blended together from many volumes).
	// Setup by the main thread, passed to the render thread and never touched again by the main thread.

	// Set values before any override happens.
	FinalPostProcessSettings.SetBaseValues();

	// project settings might want to have different defaults
	{
		if(!CVarDefaultBloom.GetValueOnGameThread())
		{
			FinalPostProcessSettings.BloomIntensity = 0;
		}
		if (!CVarDefaultAmbientOcclusion.GetValueOnGameThread())
		{
			FinalPostProcessSettings.AmbientOcclusionIntensity = 0;
		}
		if (!CVarDefaultAutoExposure.GetValueOnGameThread())
		{
			FinalPostProcessSettings.AutoExposureMinBrightness = 1;
			FinalPostProcessSettings.AutoExposureMaxBrightness = 1;
		}
		else 
		{
			int32 Value = CVarDefaultAutoExposureMethod.GetValueOnGameThread();
			if (Value >= 0 && Value < AEM_MAX)
			{
				FinalPostProcessSettings.AutoExposureMethod = (EAutoExposureMethod)Value;
			}
		}

		if (!CVarDefaultMotionBlur.GetValueOnGameThread())
		{
			FinalPostProcessSettings.MotionBlurAmount = 0;
		}
		if (!CVarDefaultLensFlare.GetValueOnGameThread())
		{
			FinalPostProcessSettings.LensFlareIntensity = 0;
		}

		{
			int32 Value = CVarDefaultAmbientOcclusionStaticFraction.GetValueOnGameThread();

			if(!Value)
			{
				FinalPostProcessSettings.AmbientOcclusionStaticFraction = 0.0f;
			}
		}
	}

	if(State)
	{
		State->OnStartPostProcessing(*this);
	}

	UWorld* World = Family->Scene->GetWorld();

	// Some views have no world (e.g. material preview)
	if (World)
	{
		for (auto VolumeIt = World->PostProcessVolumes.CreateIterator(); VolumeIt; ++VolumeIt)
		{
			DoPostProcessVolume(*VolumeIt, InViewLocation, this);
		}
	}
}

void FSceneView::EndFinalPostprocessSettings(const FSceneViewInitOptions& ViewInitOptions)
{
	const auto SceneViewFeatureLevel = GetFeatureLevel();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.EyeAdaptation.MethodOveride"));
	if (CVar->GetValueOnGameThread() == -2)
	{
		// seemed to be good setting for Paragon, we might want to remove or adjust this later on
		FinalPostProcessSettings.AutoExposureMethod = AEM_Basic;
		FinalPostProcessSettings.AutoExposureBias = -0.6f;
		FinalPostProcessSettings.AutoExposureMaxBrightness = 2.f;
		FinalPostProcessSettings.AutoExposureMinBrightness = 0.05;
		FinalPostProcessSettings.AutoExposureSpeedDown = 1.f;
		FinalPostProcessSettings.AutoExposureSpeedUp = 3.f;
	}
#endif

	// will be deprecated soon, use the new asset LightPropagationVolumeBlendable instead
	{
		FLightPropagationVolumeSettings& Dest = FinalPostProcessSettings.BlendableManager.GetSingleFinalData<FLightPropagationVolumeSettings>();

		if(Dest.LPVDirectionalOcclusionIntensity < 0.001f)
		{
			Dest.LPVDirectionalOcclusionIntensity = 0.0f;
		}

		if (Dest.LPVIntensity < 0.001f)
		{
			Dest.LPVIntensity = 0.0f;
		}

		if(!Family->EngineShowFlags.GlobalIllumination)
		{
			Dest.LPVIntensity = 0.0f;
		}
	}

	{
		static const auto SceneColorFringeQualityCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SceneColorFringeQuality"));

		int32 FringeQuality = SceneColorFringeQualityCVar->GetValueOnGameThread();
		if (FringeQuality <= 0)
		{
			FinalPostProcessSettings.SceneFringeIntensity = 0;
		}
	}

	{
		static const auto BloomQualityCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BloomQuality"));

		int Value = BloomQualityCVar->GetValueOnGameThread();

		if(Value <= 0)
		{
			FinalPostProcessSettings.BloomIntensity = 0.0f;
		}
	}

	if(!Family->EngineShowFlags.Bloom)
	{
		FinalPostProcessSettings.BloomIntensity = 0.0f;
	}

	// scale down tone mapper shader permutation
	{
		int32 Quality = CVarTonemapperQuality.GetValueOnGameThread();

		if(Quality < 1)
		{
			FinalPostProcessSettings.FilmContrast = 0;
		}

		if(Quality < 2)
		{
			FinalPostProcessSettings.VignetteIntensity = 0;
		}

		if(Quality < 3)
		{
			FinalPostProcessSettings.FilmShadowTintAmount = 0;
		}

		if(Quality < 4)
		{
			FinalPostProcessSettings.GrainIntensity = 0;
		}

		if(Quality < 5)
		{
			FinalPostProcessSettings.GrainJitter = 0;
		}
	}

	{
		static const auto DepthOfFieldQualityCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DepthOfFieldQuality"));

		int Value = DepthOfFieldQualityCVar->GetValueOnGameThread();

		if(Value <= 0)
		{
			FinalPostProcessSettings.DepthOfFieldScale = 0.0f;
		}
	}

	if(!Family->EngineShowFlags.DepthOfField)
	{
		FinalPostProcessSettings.DepthOfFieldScale = 0;
	}

	if(!Family->EngineShowFlags.Vignette)
	{
		FinalPostProcessSettings.VignetteIntensity = 0;
	}

	if(!Family->EngineShowFlags.Grain)
	{
		FinalPostProcessSettings.GrainIntensity = 0;
		FinalPostProcessSettings.GrainJitter = 0;
	}

	if(!Family->EngineShowFlags.CameraImperfections)
	{
		FinalPostProcessSettings.BloomDirtMaskIntensity = 0;
	}

	if(!Family->EngineShowFlags.AmbientCubemap)
	{
		FinalPostProcessSettings.ContributingCubemaps.Reset();
	}

	if(!Family->EngineShowFlags.LensFlares)
	{
		FinalPostProcessSettings.LensFlareIntensity = 0;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		float Value = CVarExposureOffset.GetValueOnGameThread();
		FinalPostProcessSettings.AutoExposureBias += Value;
	}

	{
		float& DepthBlurAmount = FinalPostProcessSettings.DepthOfFieldDepthBlurAmount;

		float CVarAmount = CVarDepthOfFieldDepthBlurAmount.GetValueOnGameThread();

		DepthBlurAmount = (CVarAmount > 0.0f) ? (DepthBlurAmount * CVarAmount) : -CVarAmount;
	}

	{
		float& DepthBlurRadius = FinalPostProcessSettings.DepthOfFieldDepthBlurRadius;
		{
			float CVarResScale = FMath::Max(1.0f, CVarDepthOfFieldDepthBlurResolutionScale.GetValueOnGameThread());
			
			float Factor = FMath::Max(ViewRect.Width() / 1920.0f - 1.0f, 0.0f);

			DepthBlurRadius *= 1.0f + Factor * (CVarResScale - 1.0f);
		}
		{
			float CVarScale = CVarDepthOfFieldDepthBlurScale.GetValueOnGameThread();

			DepthBlurRadius = (CVarScale > 0.0f) ? (DepthBlurRadius * CVarScale) : -CVarScale;
		}
	}
#endif

	if(FinalPostProcessSettings.DepthOfFieldMethod == DOFM_CircleDOF)
	{
		// We intentionally don't do the DepthOfFieldFocalRegion as it breaks realism.
		// Doing this fixes DOF material expression.
		FinalPostProcessSettings.DepthOfFieldFocalRegion = 0;
	}

	{
		static const auto ScreenPercentageCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));

		float Value = ScreenPercentageCVar->GetValueOnGameThread();

		if(Value >= 0.0)
		{
			FinalPostProcessSettings.ScreenPercentage *= Value / 100.0f;
		}
	}

	{
		const bool bStereoEnabled = StereoPass != eSSP_FULL;
		const bool bScaledToRenderTarget = GEngine->HMDDevice.IsValid() && bStereoEnabled;
		if (bScaledToRenderTarget)
		{
			GEngine->HMDDevice->UpdatePostProcessSettings(&FinalPostProcessSettings);
		}
	}

	{
		float Value = CVarSSRMaxRoughness.GetValueOnGameThread();

		if(Value >= 0.0f)
		{
			FinalPostProcessSettings.ScreenSpaceReflectionMaxRoughness = Value;
		}
	}

	{
		static const auto AmbientOcclusionStaticFractionCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.AmbientOcclusionStaticFraction"));

		float Value = AmbientOcclusionStaticFractionCVar->GetValueOnGameThread();

		if(Value >= 0.0)
		{
			FinalPostProcessSettings.AmbientOcclusionStaticFraction = Value;
		}
	}

	// Not supported in ES2/3.
	bool bES2Or3 = (SceneViewFeatureLevel == ERHIFeatureLevel::ES2) || (SceneViewFeatureLevel == ERHIFeatureLevel::ES3_1);

	if(!Family->EngineShowFlags.ScreenPercentage || bIsSceneCapture || bIsReflectionCapture || bES2Or3)
	{
		FinalPostProcessSettings.ScreenPercentage = 100;
	}

	if(!Family->EngineShowFlags.AmbientOcclusion || !Family->EngineShowFlags.ScreenSpaceAO)
	{
		FinalPostProcessSettings.AmbientOcclusionIntensity = 0;
	}

	{
		static const auto AmbientOcclusionRadiusScaleCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.AmbientOcclusionRadiusScale"));

		float Scale = FMath::Clamp(AmbientOcclusionRadiusScaleCVar->GetValueOnGameThread(), 0.1f, 15.0f);
		
		FinalPostProcessSettings.AmbientOcclusionRadius *= Scale;
	}

	{
		float Scale = FMath::Clamp(CVarSSAOFadeRadiusScale.GetValueOnGameThread(), 0.01f, 50.0f);

		FinalPostProcessSettings.AmbientOcclusionDistance_DEPRECATED *= Scale;
	}

	{
		float Value = FMath::Clamp(CVarMotionBlurScale.GetValueOnGameThread(), 0.0f, 50.0f);

		FinalPostProcessSettings.MotionBlurAmount *= Value;
	}

	{
		float Value = CVarMotionBlurAmount.GetValueOnGameThread();

		if(Value >= 0.0f)
		{
			FinalPostProcessSettings.MotionBlurAmount = Value;
		}
	}

	{
		float Value = CVarMotionBlurMax.GetValueOnGameThread();

		if(Value >= 0.0f)
		{
			FinalPostProcessSettings.MotionBlurMax = Value;
		}
	}

	{
		float Value = CVarSceneColorFringeMax.GetValueOnGameThread();

		if (Value >= 0.0f)
		{
			FinalPostProcessSettings.SceneFringeIntensity = FMath::Min(FinalPostProcessSettings.SceneFringeIntensity, Value);
		}
		else if (Value == -2.0f)
		{
			FinalPostProcessSettings.SceneFringeIntensity = 5.0f;
		}

		if(!Family->EngineShowFlags.SceneColorFringe || !Family->EngineShowFlags.CameraImperfections)
		{
			FinalPostProcessSettings.SceneFringeIntensity = 0;
		}
	}

	if (!Family->EngineShowFlags.Lighting || !Family->EngineShowFlags.GlobalIllumination)
	{
		FinalPostProcessSettings.IndirectLightingColor = FLinearColor(0,0,0,0);
		FinalPostProcessSettings.IndirectLightingIntensity = 0.0f;
	}

	if (AllowDebugViewmodes())
	{
		ConfigureBufferVisualizationSettings();
	}

#if WITH_EDITOR
	FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();

	// Pass highres screenshot materials through post process settings
	FinalPostProcessSettings.HighResScreenshotMaterial = Config.HighResScreenshotMaterial;
	FinalPostProcessSettings.HighResScreenshotMaskMaterial = Config.HighResScreenshotMaskMaterial;
	FinalPostProcessSettings.HighResScreenshotCaptureRegionMaterial = NULL;

	// If the highres screenshot UI is open and we're not taking a highres screenshot this frame
	if (Config.bDisplayCaptureRegion && !GIsHighResScreenshot)
	{
		// Only enable the capture region effect if the capture region is different from the view rectangle...
		if ((Config.UnscaledCaptureRegion != ViewRect) && (Config.UnscaledCaptureRegion.Area() > 0) && (State != NULL))
		{
			// ...and if this is the viewport associated with the highres screenshot UI
			auto ConfigViewport = Config.TargetViewport.Pin();
			if (ConfigViewport.IsValid() && Family->RenderTarget == ConfigViewport->GetViewport())
			{
				static const FName ParamName = "RegionRect";
				FLinearColor NormalizedCaptureRegion;

				// Normalize capture region into view rectangle
				NormalizedCaptureRegion.R = (float)Config.UnscaledCaptureRegion.Min.X / (float)ViewRect.Width();
				NormalizedCaptureRegion.G = (float)Config.UnscaledCaptureRegion.Min.Y / (float)ViewRect.Height();
				NormalizedCaptureRegion.B = (float)Config.UnscaledCaptureRegion.Max.X / (float)ViewRect.Width();
				NormalizedCaptureRegion.A = (float)Config.UnscaledCaptureRegion.Max.Y / (float)ViewRect.Height();

				// Get a MID for drawing this frame and push the capture region into the shader parameter
				FinalPostProcessSettings.HighResScreenshotCaptureRegionMaterial = State->GetReusableMID(Config.HighResScreenshotCaptureRegionMaterial);
				FinalPostProcessSettings.HighResScreenshotCaptureRegionMaterial->SetVectorParameterValue(ParamName, NormalizedCaptureRegion);
			}
		}
	}
#endif // WITH_EDITOR


	// Upscaling or Super sampling
	{
		float LocalScreenPercentage = FinalPostProcessSettings.ScreenPercentage;

		float Fraction = 1.0f;

		// apply ScreenPercentage
		if (LocalScreenPercentage != 100.f)
		{
			Fraction = FMath::Clamp(LocalScreenPercentage / 100.0f, 0.1f, 4.0f);
		}

		// Window full screen mode with upscaling
		bool bFullscreen = false;
		bool bSceneCapture = false;
		if (ViewInitOptions.ViewFamily && ViewInitOptions.ViewFamily->Views.Num() == 1)
		{
			auto* View = ViewInitOptions.ViewFamily->Views[0];
			bSceneCapture = View ? View->bIsSceneCapture : false;
		}

		if (!bSceneCapture && GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
		{												  
			bFullscreen = GEngine->GameViewport->GetWindow()->GetWindowMode() != EWindowMode::Windowed;
		}

		check(Family->RenderTarget);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if(CVarScreenPercentageEditor.GetValueOnAnyThread() == 0)
		{
			// Don't apply editor screen percentage scaling while in a PIE viewport
			const bool bInGame = !GEngine || GEngine->GameViewport != nullptr;

			// Don't apply editor screen percentage scaling while rendering a stereo view in the editor, as HMDs often
			// require device-specific upscaling to look correct
			const bool bStereo = ViewInitOptions.StereoPass != eSSP_FULL;

			if(!bInGame && !bStereo)
			{
				Fraction = 1.0f;
			}
		}
#endif


		// Upscale if needed
		if (Fraction != 1.0f)
		{
			// compute the view rectangle with the ScreenPercentage applied
			const FIntRect ScreenPercentageAffectedViewRect = ViewInitOptions.GetConstrainedViewRect().Scale(Fraction);
			SetScaledViewRect(ScreenPercentageAffectedViewRect);
		}
	}
}

void FSceneView::ConfigureBufferVisualizationSettings()
{
	bool bBufferDumpingRequired = (FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot || GIsDumpingMovie);
	bool bVisualizationRequired = Family->EngineShowFlags.VisualizeBuffer;
	
	if (bVisualizationRequired || bBufferDumpingRequired)
	{
		FinalPostProcessSettings.bBufferVisualizationDumpRequired = bBufferDumpingRequired;
		FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Empty();

		if (bBufferDumpingRequired)
		{
			FinalPostProcessSettings.BufferVisualizationDumpBaseFilename = FPaths::GetBaseFilename(FScreenshotRequest::GetFilename(), false);
		}

		// Get the list of requested buffers from the console
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationOverviewTargets"));
		FString SelectedMaterialNames = CVar->GetString();

		FBufferVisualizationData& BufferVisualizationData = GetBufferVisualizationData();

		if (BufferVisualizationData.IsDifferentToCurrentOverviewMaterialNames(SelectedMaterialNames))
		{
			FString Left, Right;

			// Update our record of the list of materials we've been asked to display
			BufferVisualizationData.SetCurrentOverviewMaterialNames(SelectedMaterialNames);
			BufferVisualizationData.GetOverviewMaterials().Empty();

			// Extract each material name from the comma separated string
			while (SelectedMaterialNames.Len())
			{
				// Detect last entry in the list
				if (!SelectedMaterialNames.Split(TEXT(","), &Left, &Right))
				{
					Left = SelectedMaterialNames;
					Right = FString();
				}

				// Lookup this material from the list that was parsed out of the global ini file
				Left = Left.Trim();
				UMaterial* Material = BufferVisualizationData.GetMaterial(*Left);

				if (Material == NULL && Left.Len() > 0)
				{
					UE_LOG(LogBufferVisualization, Warning, TEXT("Unknown material '%s'"), *Left);
				}

				// Add this material into the material list in the post processing settings so that the render thread
				// can pick them up and draw them into the on-screen tiles
				BufferVisualizationData.GetOverviewMaterials().Add(Material);
				
				SelectedMaterialNames = Right;
			}
		}

		// Copy current material list into settings material list
		for (TArray<UMaterial*>::TConstIterator It = BufferVisualizationData.GetOverviewMaterials().CreateConstIterator(); It; ++It)
		{
			FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Add(*It);
		}
	}
}

EShaderPlatform FSceneView::GetShaderPlatform() const
{
	return GShaderPlatformForFeatureLevel[GetFeatureLevel()];
}

void FSceneView::SetupViewRectUniformBufferParameters(const FIntPoint& BufferSize, const FIntRect& EffectiveViewRect, FViewUniformShaderParameters& ViewUniformShaderParameters) const
{
	checkfSlow(EffectiveViewRect.Area() > 0, TEXT("Invalid-size EffectiveViewRect passed to CreateUniformBufferParameters [%d * %d]."), EffectiveViewRect.Width(), EffectiveViewRect.Height());

	// Calculate the vector used by shaders to convert clip space coordinates to texture space.
	const float InvBufferSizeX = 1.0f / BufferSize.X;
	const float InvBufferSizeY = 1.0f / BufferSize.Y;
	// to bring NDC (-1..1, 1..-1) into 0..1 UV for BufferSize textures
	const FVector4 ScreenPositionScaleBias(
		EffectiveViewRect.Width() * InvBufferSizeX / +2.0f,
		EffectiveViewRect.Height() * InvBufferSizeY / (-2.0f * GProjectionSignY),
		(EffectiveViewRect.Height() / 2.0f + EffectiveViewRect.Min.Y) * InvBufferSizeY,
		(EffectiveViewRect.Width() / 2.0f + EffectiveViewRect.Min.X) * InvBufferSizeX
		);

	ViewUniformShaderParameters.ScreenPositionScaleBias = ScreenPositionScaleBias;

	ViewUniformShaderParameters.ViewRectMin = FVector4(EffectiveViewRect.Min.X, EffectiveViewRect.Min.Y, 0.0f, 0.0f);
	ViewUniformShaderParameters.ViewSizeAndInvSize = FVector4(EffectiveViewRect.Width(), EffectiveViewRect.Height(), 1.0f / float(EffectiveViewRect.Width()), 1.0f / float(EffectiveViewRect.Height()));
	ViewUniformShaderParameters.BufferSizeAndInvSize = FVector4(BufferSize.X, BufferSize.Y, InvBufferSizeX, InvBufferSizeY);

	FVector2D OneScenePixelUVSize = FVector2D(1.0f / BufferSize.X, 1.0f / BufferSize.Y);
	FVector4 SceneTexMinMax(((float)EffectiveViewRect.Min.X / BufferSize.X),
		((float)EffectiveViewRect.Min.Y / BufferSize.Y),
		(((float)EffectiveViewRect.Max.X / BufferSize.X) - OneScenePixelUVSize.X),
		(((float)EffectiveViewRect.Max.Y / BufferSize.Y) - OneScenePixelUVSize.Y));

	ViewUniformShaderParameters.SceneTextureMinMax = SceneTexMinMax;

	ViewUniformShaderParameters.MotionBlurNormalizedToPixel = FinalPostProcessSettings.MotionBlurMax * EffectiveViewRect.Width() / 100.0f;

	{
		// setup a matrix to transform float4(SvPosition.xyz,1) directly to TranslatedWorld (quality, performance as we don't need to convert or use interpolator)

		//	new_xy = (xy - ViewRectMin.xy) * ViewSizeAndInvSize.zw * float2(2,-2) + float2(-1, 1);

		//  transformed into one MAD:  new_xy = xy * ViewSizeAndInvSize.zw * float2(2,-2)      +       (-ViewRectMin.xy) * ViewSizeAndInvSize.zw * float2(2,-2) + float2(-1, 1);

		float Mx = 2.0f * ViewUniformShaderParameters.ViewSizeAndInvSize.Z;
		float My = -2.0f * ViewUniformShaderParameters.ViewSizeAndInvSize.W;
		float Ax = -1.0f - 2.0f * EffectiveViewRect.Min.X * ViewUniformShaderParameters.ViewSizeAndInvSize.Z;
		float Ay = 1.0f + 2.0f * EffectiveViewRect.Min.Y * ViewUniformShaderParameters.ViewSizeAndInvSize.W;

		// http://stackoverflow.com/questions/9010546/java-transformation-matrix-operations

		ViewUniformShaderParameters.SVPositionToTranslatedWorld =
			FMatrix(FPlane(Mx, 0, 0, 0),
				FPlane(0, My, 0, 0),
				FPlane(0, 0, 1, 0),
				FPlane(Ax, Ay, 0, 1)) * ViewMatrices.InvTranslatedViewProjectionMatrix;
	}


	if(Family->EngineShowFlags.Tessellation)
	{
		// CVar setting is pixels/tri which is nice and intuitive.  But we want pixels/tessellated edge.  So use a heuristic.
		float TessellationAdaptivePixelsPerEdge = FMath::Sqrt(2.f * CVarTessellationAdaptivePixelsPerTriangle.GetValueOnRenderThread());

		ViewUniformShaderParameters.AdaptiveTessellationFactor = 0.5f * ViewMatrices.ProjMatrix.M[1][1] * float(EffectiveViewRect.Height()) / TessellationAdaptivePixelsPerEdge;
	}

}

void FSceneView::SetupCommonViewUniformBufferParameters(
	FViewUniformShaderParameters& ViewUniformShaderParameters,
	const FIntPoint& BufferSize, 
	const FIntRect& EffectiveViewRect, 
	const FMatrix& EffectiveTranslatedViewMatrix, 
	const FMatrix& EffectiveViewToTranslatedWorld, 
	const FViewMatrices& PrevViewMatrices,
	const FMatrix& PrevViewProjMatrix, 
	const FMatrix& PrevViewRotationProjMatrix) const
{
	SetupViewRectUniformBufferParameters(BufferSize, EffectiveViewRect, ViewUniformShaderParameters);

	FVector4 LocalDiffuseOverrideParameter = DiffuseOverrideParameter;
	FVector2D LocalRoughnessOverrideParameter = RoughnessOverrideParameter;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		// assuming we have no color in the multipliers
		float MinValue = LocalDiffuseOverrideParameter.X;
		float MaxValue = MinValue + LocalDiffuseOverrideParameter.W;

		float NewMinValue = FMath::Max(MinValue, CVarDiffuseColorMin.GetValueOnRenderThread());
		float NewMaxValue = FMath::Min(MaxValue, CVarDiffuseColorMax.GetValueOnRenderThread());

		LocalDiffuseOverrideParameter.X = LocalDiffuseOverrideParameter.Y = LocalDiffuseOverrideParameter.Z = NewMinValue;
		LocalDiffuseOverrideParameter.W = NewMaxValue - NewMinValue;
	}
	{
		float MinValue = LocalRoughnessOverrideParameter.X;
		float MaxValue = MinValue + LocalRoughnessOverrideParameter.Y;

		float NewMinValue = FMath::Max(MinValue, CVarRoughnessMin.GetValueOnRenderThread());
		float NewMaxValue = FMath::Min(MaxValue, CVarRoughnessMax.GetValueOnRenderThread());

		LocalRoughnessOverrideParameter.X = NewMinValue;
		LocalRoughnessOverrideParameter.Y = NewMaxValue - NewMinValue;
	}

#endif

	ViewUniformShaderParameters.ViewToTranslatedWorld = EffectiveViewToTranslatedWorld;
	ViewUniformShaderParameters.TranslatedWorldToClip = ViewMatrices.TranslatedViewProjectionMatrix;
	ViewUniformShaderParameters.WorldToClip = ViewProjectionMatrix;
	ViewUniformShaderParameters.TranslatedWorldToView = EffectiveTranslatedViewMatrix;
	ViewUniformShaderParameters.TranslatedWorldToCameraView = ViewMatrices.TranslatedViewMatrix;
	ViewUniformShaderParameters.CameraViewToTranslatedWorld = ViewUniformShaderParameters.TranslatedWorldToCameraView.Inverse();
	ViewUniformShaderParameters.ViewToClip = ViewMatrices.ProjMatrix;
	ViewUniformShaderParameters.ClipToView = ViewMatrices.GetInvProjMatrix();
	ViewUniformShaderParameters.ClipToTranslatedWorld = ViewMatrices.InvTranslatedViewProjectionMatrix;
	ViewUniformShaderParameters.ViewForward = EffectiveTranslatedViewMatrix.GetColumn(2);
	ViewUniformShaderParameters.ViewUp = EffectiveTranslatedViewMatrix.GetColumn(1);
	ViewUniformShaderParameters.ViewRight = EffectiveTranslatedViewMatrix.GetColumn(0);
	ViewUniformShaderParameters.HMDViewNoRollUp = ViewMatrices.HMDViewMatrixNoRoll.GetColumn(1);
	ViewUniformShaderParameters.HMDViewNoRollRight = ViewMatrices.HMDViewMatrixNoRoll.GetColumn(0);
	ViewUniformShaderParameters.InvDeviceZToWorldZTransform = InvDeviceZToWorldZTransform;
	ViewUniformShaderParameters.WorldViewOrigin = EffectiveViewToTranslatedWorld.TransformPosition(FVector(0)) - ViewMatrices.PreViewTranslation;
	ViewUniformShaderParameters.WorldCameraOrigin = ViewMatrices.ViewOrigin;
	ViewUniformShaderParameters.TranslatedWorldCameraOrigin = ViewMatrices.ViewOrigin + ViewMatrices.PreViewTranslation;
	ViewUniformShaderParameters.PreViewTranslation = ViewMatrices.PreViewTranslation;
	ViewUniformShaderParameters.PrevProjection = PrevViewMatrices.ProjMatrix;
	ViewUniformShaderParameters.PrevViewProj = PrevViewProjMatrix;
	ViewUniformShaderParameters.PrevViewRotationProj = PrevViewRotationProjMatrix;
	ViewUniformShaderParameters.PrevViewToClip = PrevViewMatrices.ProjMatrix;
	ViewUniformShaderParameters.PrevClipToView = PrevViewMatrices.GetInvProjMatrix();
	ViewUniformShaderParameters.PrevTranslatedWorldToClip = PrevViewMatrices.TranslatedViewProjectionMatrix;
	// EffectiveTranslatedViewMatrix != ViewMatrices.TranslatedViewMatrix in the shadow pass
	// and we don't have EffectiveTranslatedViewMatrix for the previous frame to set up PrevTranslatedWorldToView
	// but that is fine to set up PrevTranslatedWorldToView as same as PrevTranslatedWorldToCameraView
	// since the shadow pass doesn't require previous frame computation.
	ViewUniformShaderParameters.PrevTranslatedWorldToView = PrevViewMatrices.TranslatedViewMatrix;
	ViewUniformShaderParameters.PrevViewToTranslatedWorld = ViewUniformShaderParameters.PrevTranslatedWorldToView.Inverse();
	ViewUniformShaderParameters.PrevTranslatedWorldToCameraView = PrevViewMatrices.TranslatedViewMatrix;
	ViewUniformShaderParameters.PrevCameraViewToTranslatedWorld = ViewUniformShaderParameters.PrevTranslatedWorldToCameraView.Inverse();
	ViewUniformShaderParameters.PrevWorldCameraOrigin = PrevViewMatrices.ViewOrigin;
	// previous view world origin is going to be needed only in the base pass or shadow pass
	// therefore is same as previous camera world origin.
	ViewUniformShaderParameters.PrevWorldViewOrigin = ViewUniformShaderParameters.PrevWorldCameraOrigin;
	ViewUniformShaderParameters.PrevPreViewTranslation = PrevViewMatrices.PreViewTranslation;
	// can be optimized
	ViewUniformShaderParameters.PrevInvViewProj = PrevViewProjMatrix.Inverse();
	ViewUniformShaderParameters.GlobalClippingPlane = FVector4(GlobalClippingPlane.X, GlobalClippingPlane.Y, GlobalClippingPlane.Z, -GlobalClippingPlane.W);

	ViewUniformShaderParameters.FieldOfViewWideAngles = 2.f * ViewMatrices.GetHalfFieldOfViewPerAxis();
	ViewUniformShaderParameters.PrevFieldOfViewWideAngles = 2.f * PrevViewMatrices.GetHalfFieldOfViewPerAxis();
	ViewUniformShaderParameters.DiffuseOverrideParameter = LocalDiffuseOverrideParameter;
	ViewUniformShaderParameters.SpecularOverrideParameter = SpecularOverrideParameter;
	ViewUniformShaderParameters.NormalOverrideParameter = NormalOverrideParameter;
	ViewUniformShaderParameters.RoughnessOverrideParameter = LocalRoughnessOverrideParameter;
	ViewUniformShaderParameters.PrevFrameGameTime = Family->CurrentWorldTime - Family->DeltaWorldTime;
	ViewUniformShaderParameters.PrevFrameRealTime = Family->CurrentRealTime - Family->DeltaWorldTime;
	ViewUniformShaderParameters.WorldCameraMovementSinceLastFrame = ViewMatrices.ViewOrigin - PrevViewMatrices.ViewOrigin;
	ViewUniformShaderParameters.CullingSign = bReverseCulling ? -1.0f : 1.0f;
	ViewUniformShaderParameters.NearPlane = GNearClippingPlane;

	ViewUniformShaderParameters.bCheckerboardSubsurfaceProfileRendering = 0;

	ViewUniformShaderParameters.ScreenToWorld = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, ProjectionMatrixUnadjustedForRHI.M[2][2], 1),
		FPlane(0, 0, ProjectionMatrixUnadjustedForRHI.M[3][2], 0))
		* InvViewProjectionMatrix;

	ViewUniformShaderParameters.ScreenToTranslatedWorld = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, ProjectionMatrixUnadjustedForRHI.M[2][2], 1),
		FPlane(0, 0, ProjectionMatrixUnadjustedForRHI.M[3][2], 0))
		* ViewMatrices.InvTranslatedViewProjectionMatrix;

	ViewUniformShaderParameters.PrevScreenToTranslatedWorld = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, ProjectionMatrixUnadjustedForRHI.M[2][2], 1),
		FPlane(0, 0, ProjectionMatrixUnadjustedForRHI.M[3][2], 0))
		* PrevViewMatrices.InvTranslatedViewProjectionMatrix;

	FVector DeltaTranslation = PrevViewMatrices.PreViewTranslation - ViewMatrices.PreViewTranslation;
	FMatrix InvViewProj = ViewMatrices.GetInvProjNoAAMatrix() * ViewMatrices.TranslatedViewMatrix.GetTransposed();
	FMatrix PrevViewProj = FTranslationMatrix(DeltaTranslation) * PrevViewMatrices.TranslatedViewMatrix * PrevViewMatrices.GetProjNoAAMatrix();

	ViewUniformShaderParameters.ClipToPrevClip = InvViewProj * PrevViewProj;

	// is getting clamped in the shader to a value larger than 0 (we don't want the triangles to disappear)
	ViewUniformShaderParameters.AdaptiveTessellationFactor = 0.0f;

	ViewUniformShaderParameters.UnlitViewmodeMask = !Family->EngineShowFlags.Lighting ? 1 : 0;
	ViewUniformShaderParameters.OutOfBoundsMask = Family->EngineShowFlags.VisualizeOutOfBoundsPixels ? 1 : 0;

	ViewUniformShaderParameters.GameTime = Family->CurrentWorldTime;
	ViewUniformShaderParameters.RealTime = Family->CurrentRealTime;
	ViewUniformShaderParameters.Random = FMath::Rand();
	ViewUniformShaderParameters.FrameNumber = Family->FrameNumber;

	ViewUniformShaderParameters.CameraCut = bCameraCut ? 1 : 0;
}

FSceneViewFamily::FSceneViewFamily(const ConstructionValues& CVS)
	:
	FamilySizeX(0),
	FamilySizeY(0),
	InstancedStereoWidth(0),
	RenderTarget(CVS.RenderTarget),
	bUseSeparateRenderTarget(false),
	Scene(CVS.Scene),
	EngineShowFlags(CVS.EngineShowFlags),
	CurrentWorldTime(CVS.CurrentWorldTime),
	DeltaWorldTime(CVS.DeltaWorldTime),
	CurrentRealTime(CVS.CurrentRealTime),
	FrameNumber(UINT_MAX),
	bRealtimeUpdate(CVS.bRealtimeUpdate),
	bDeferClear(CVS.bDeferClear),
	bResolveScene(CVS.bResolveScene),
	SceneCaptureSource(SCS_FinalColorLDR),
	SceneCaptureCompositeMode(SCCM_Overwrite),
	bWorldIsPaused(false),
	GammaCorrection(CVS.GammaCorrection)
{
	// If we do not pass a valid scene pointer then SetWorldTimes must be called to initialized with valid times.
	ensure(CVS.bTimesSet);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	int32 Value = CVarRenderTimeFrozen.GetValueOnAnyThread();
	if(Value)
	{
		CurrentWorldTime = 0;
		CurrentRealTime = 0; 
	}

	DebugViewShaderMode = ChooseDebugViewShaderMode();
	if (!AllowDebugViewPS(DebugViewShaderMode, GetShaderPlatform()))
	{
		DebugViewShaderMode = DVSM_None;
	}
	bUsedDebugViewPSVSHS = DebugViewShaderMode != DVSM_None && AllowDebugViewVSDSHS(GetShaderPlatform());
#endif

#if !WITH_EDITOR
	check(!EngineShowFlags.StationaryLightOverlap);
#else

	// instead of checking IsGameWorld on rendering thread to see if we allow this flag to be disabled 
	// we force it on in the game thread.
	if(IsInGameThread() && Scene)
	{
		UWorld* World = Scene->GetWorld();

		if (World)
		{
			if (World->IsGameWorld())
			{
				EngineShowFlags.LOD = 1;
			}

			if (World->IsPaused())
			{
				// to fix UE-17047 Motion Blur exaggeration when Paused in Simulate
				bool bCameraCanMove = GEditor && GEditor->bIsSimulatingInEditor;
				if(!bCameraCanMove)
				{
					bWorldIsPaused = true;
				}
			}
		}
	}

	LandscapeLODOverride = -1;
	bDrawBaseInfo = true;
	bNullifyWorldSpacePosition = false;
#endif
}

void FSceneViewFamily::ComputeFamilySize()
{
	// Calculate the screen extents of the view family.
	bool bInitializedExtents = false;
	float MaxFamilyX = 0;
	float MaxFamilyY = 0;

	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];

		if (View->ResolutionOverrideRect.Area() > 0)
		{
			MaxFamilyX = FMath::Max(MaxFamilyX, static_cast<float>(View->ResolutionOverrideRect.Max.X));
			MaxFamilyY = FMath::Max(MaxFamilyY, static_cast<float>(View->ResolutionOverrideRect.Max.Y));
			bInitializedExtents = true;
		}
		else
		{
			float FinalViewMaxX = (float)View->ViewRect.Max.X;
			float FinalViewMaxY = (float)View->ViewRect.Max.Y;

			// Derive the amount of scaling needed for screenpercentage from the scaled / unscaled rect
			const float XScale = FinalViewMaxX / (float)View->UnscaledViewRect.Max.X;
			const float YScale = FinalViewMaxY / (float)View->UnscaledViewRect.Max.Y;

			if (!bInitializedExtents)
			{
				// Note: using the unconstrained view rect to compute family size
				// In the case of constrained views (black bars) this means the scene render targets will fill the whole screen
				// Which is needed for ES2 paths where we render directly to the backbuffer, and the scene depth buffer has to match in size
				MaxFamilyX = View->UnconstrainedViewRect.Max.X * XScale;
				MaxFamilyY = View->UnconstrainedViewRect.Max.Y * YScale;
				bInitializedExtents = true;
			}
			else
			{
				MaxFamilyX = FMath::Max(MaxFamilyX, View->UnconstrainedViewRect.Max.X * XScale);
				MaxFamilyY = FMath::Max(MaxFamilyY, View->UnconstrainedViewRect.Max.Y * YScale);
			}

			// floating point imprecision could cause MaxFamilyX to be less than View->ViewRect.Max.X after integer truncation.
			// since this value controls rendertarget sizes, we don't want to create rendertargets smaller than the view size.
			MaxFamilyX = FMath::Max(MaxFamilyX, FinalViewMaxX);
			MaxFamilyY = FMath::Max(MaxFamilyY, FinalViewMaxY);
		}

		InstancedStereoWidth = FPlatformMath::Max(InstancedStereoWidth, static_cast<uint32>(Views[ViewIndex]->ViewRect.Max.X));
	}

	// We render to the actual position of the viewports so with black borders we need the max.
	// We could change it by rendering all to left top but that has implications for splitscreen. 
	FamilySizeX = FMath::TruncToInt(MaxFamilyX);
	FamilySizeY = FMath::TruncToInt(MaxFamilyY);	

	check(bInitializedExtents);
}

ERHIFeatureLevel::Type FSceneViewFamily::GetFeatureLevel() const
{ 
	if (Scene)
	{
		return Scene->GetFeatureLevel();
	}
	else
	{
		return GMaxRHIFeatureLevel;
	}
}

const FSceneView& FSceneViewFamily::GetStereoEyeView(const EStereoscopicPass Eye) const
{
	const int32 EyeIndex = static_cast<int32>(Eye);
	check(Views.Num() > 0 && Views.Num() >= EyeIndex);

	// Mono or left eye
	if (EyeIndex <= 1)
	{
		return *Views[0];
	}

	// Right eye
	else
	{
		return *Views[1];
	}
}

FSceneViewFamilyContext::~FSceneViewFamilyContext()
{
	// Cleanup the views allocated for this view family.
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		delete Views[ViewIndex];
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

EDebugViewShaderMode FSceneViewFamily::ChooseDebugViewShaderMode() const
{
	if (EngineShowFlags.ShaderComplexity)
	{
		if (EngineShowFlags.QuadOverdraw)
		{
			return DVSM_QuadComplexity;
		}
		else if (EngineShowFlags.ShaderComplexityWithQuadOverdraw)
		{
			return DVSM_ShaderComplexityContainedQuadOverhead;
		}
		else
		{
			return DVSM_ShaderComplexity;
		}
	}
	else if (EngineShowFlags.PrimitiveDistanceAccuracy)
	{
		return DVSM_PrimitiveDistanceAccuracy;
	}
	else if (EngineShowFlags.MeshTexCoordSizeAccuracy)
	{
		return DVSM_MeshTexCoordSizeAccuracy;
	}
	else if (EngineShowFlags.MaterialTexCoordScalesAnalysis) // Test before accuracy is set since accuracy could also be set.
	{
		return DVSM_MaterialTexCoordScalesAnalysis;
	}
	else if (EngineShowFlags.MaterialTexCoordScalesAccuracy)
	{
		return DVSM_MaterialTexCoordScalesAccuracy;
	}
	return DVSM_None;
}

static bool PlatformSupportsDebugViewShaders(EShaderPlatform Platform)
{
	// List of platforms that have been tested and proven functional. 
	return Platform == SP_PCD3D_SM4 || Platform == SP_PCD3D_SM5 || Platform == SP_OPENGL_SM4 || Platform == SP_OPENGL_SM4_MAC || Platform == SP_METAL_SM4;
}

bool AllowDebugViewPS(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform)
{
#if WITH_EDITOR
	// Those options are used to test compilation on specific platforms
	static const bool bForceQuadOverdraw = FParse::Param(FCommandLine::Get(), TEXT("quadoverdraw"));
	static const bool bForceStreamingAccuracy = FParse::Param(FCommandLine::Get(), TEXT("streamingaccuracy"));
	static const bool bForceTextureStreamingBuild = FParse::Param(FCommandLine::Get(), TEXT("streamingbuild"));

	switch (ShaderMode)
	{
	case DVSM_None:
		return false;
	case DVSM_ShaderComplexity:
		return true;
	case DVSM_ShaderComplexityContainedQuadOverhead:
	case DVSM_ShaderComplexityBleedingQuadOverhead:
	case DVSM_QuadComplexity:
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && (bForceQuadOverdraw || PlatformSupportsDebugViewShaders(Platform));
	case DVSM_PrimitiveDistanceAccuracy:
	case DVSM_MeshTexCoordSizeAccuracy:
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && (bForceStreamingAccuracy || PlatformSupportsDebugViewShaders(Platform));
	case DVSM_MaterialTexCoordScalesAccuracy:
	case DVSM_MaterialTexCoordScalesAnalysis:
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && (bForceTextureStreamingBuild || PlatformSupportsDebugViewShaders(Platform));
	default:
		return false;
	}
#else
	return ShaderMode == DVSM_ShaderComplexity;
#endif
}

bool AllowDebugViewVSDSHS(EShaderPlatform Platform)
{
#if WITH_EDITOR
	// Those options are used to test compilation on specific platforms
	static const bool bForce = 
		FParse::Param(FCommandLine::Get(), TEXT("quadoverdraw")) || 
		FParse::Param(FCommandLine::Get(), TEXT("streamingaccuracy")) || 
		FParse::Param(FCommandLine::Get(), TEXT("streamingbuild"));

	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && (bForce || PlatformSupportsDebugViewShaders(Platform));
#else
	return false;
#endif
}

bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode)
{
	return AllowDebugViewPS(ShaderMode, GMaxRHIShaderPlatform);
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)