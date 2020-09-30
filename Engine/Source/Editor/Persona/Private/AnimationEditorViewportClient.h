// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"

class FAdvancedPreviewScene;
struct FCompactHeapPose;

//////////////////////////////////////////////////////////////////////////
// ELocalAxesMode

namespace ELocalAxesMode
{
	enum Type
	{
		None,
		Selected,
		All,
		NumAxesModes
	};
};

//////////////////////////////////////////////////////////////////////////
// EBoneDrawMode

namespace EBoneDrawMode
{
	enum Type
	{
		None,
		Selected,
		All,
		NumAxesModes
	};
};

//////////////////////////////////////////////////////////////////////////
// ELocalAxesMode

namespace EDisplayInfoMode
{
	enum Type
	{
		None,
		Basic,	
		Detailed,
		SkeletalControls,
		NumInfoModes
	};
};

namespace EAnimationPlaybackSpeeds
{
	enum Type
	{
		OneTenth = 0,
		Quarter,
		Half,
		Normal,
		Double,
		FiveTimes,
		TenTimes,
		NumPlaybackSpeeds
	};

	extern float Values[NumPlaybackSpeeds];
};

/**
* A hit proxy class for sockets in the Persona viewport.
*/
struct HPersonaSocketProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	FSelectedSocketInfo SocketInfo;

	explicit HPersonaSocketProxy(FSelectedSocketInfo InSocketInfo)
		: SocketInfo(InSocketInfo)
	{}
};

/**
* A hit proxy class for sockets in the Persona viewport.
*/
struct HPersonaBoneProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	FName BoneName;

	explicit HPersonaBoneProxy(const FName& InBoneName)
		: BoneName(InBoneName)
	{}
};

/////////////////////////////////////////////////////////////////////////
// FAnimationViewportClient

class FAnimationViewportClient : public FEditorViewportClient
{
protected:

	/** Function to display bone names*/
	void ShowBoneNames( FCanvas* Canvas, FSceneView* View );

	/** Function to display warning and info text on the viewport when outside of animBP mode */
	void DisplayInfo( FCanvas* Canvas, FSceneView* View, bool bDisplayAllInfo );

	/** Function to display debug lines generated from skeletal controls in animBP mode */
	void DrawNodeDebugLines(TArray<FText>& Lines, FCanvas* Canvas, FSceneView* View);

public:
	FAnimationViewportClient(const TSharedRef<class ISkeletonTree>& InSkeletonTree, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, const TSharedRef<class SAnimationEditorViewport>& InAnimationEditorViewport, const TSharedRef<class FAssetEditorToolkit>& InAssetEditorToolkit, bool bInShowStats);
	virtual ~FAnimationViewportClient();

	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas ) override;
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad=false) override;
	virtual bool InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) override;
//	virtual void ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
//	virtual bool InputWidgetDelta( FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale ) override;
	virtual void TrackingStarted( const struct FInputEventState& InInputState, bool bIsDragging, bool bNudge ) override;
	virtual void TrackingStopped() override;
//	virtual FWidget::EWidgetMode GetWidgetMode() const override;
//	virtual void SetWidgetMode(FWidget::EWidgetMode InWidgetMode) override;
//	virtual bool CanSetWidgetMode(FWidget::EWidgetMode NewMode) const override;
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const override;
	virtual void SetWidgetCoordSystemSpace(ECoordSystem NewCoordSystem) override;
	virtual void SetViewMode(EViewModeIndex InViewModeIndex) override;
	virtual void SetViewportType(ELevelViewportType InViewportType) override;
	virtual void RotateViewportType() override;
	// End of FEditorViewportClient interface

	/** Draw call to render UV overlay */
	void DrawUVsForMesh(FViewport* InViewport, FCanvas* InCanvas, int32 InTextYPos);

	/** Callback for toggling the camera lock flag. */
	virtual void SetCameraFollow();

	/** Callback for checking the camera lock flag. */
	bool IsSetCameraFollowChecked() const;

	/** Handle the skeletal mesh mesh component being used for preview changing */
	void HandleSkeletalMeshChanged(class USkeletalMesh* InSkeletalMesh);

	/** Function to display bone names*/
	void ShowBoneNames(FViewport* Viewport, FCanvas* Canvas);

	/** Function to show/hide grid in the viewport */
	void OnToggleShowGrid();

	/** Function to check whether grid is displayed or not */
	bool IsShowingGrid() const;

	/** Function to enable/disable floor auto align */
	void OnToggleAutoAlignFloor();

	/** Function to check whether floor is auto align or not */
	bool IsAutoAlignFloor() const;

	/** Function to mute/unmute audio in the viewport */
	void OnToggleMuteAudio();

	/** Function to check whether audio is muted or not */
	bool IsAudioMuted() const;

	/** Function to set background color */
	void SetBackgroundColor(FLinearColor InColor);

	/** Function to get current brightness value */ 
	float GetBrightnessValue() const;

	/** Function to set brightness value */
	void SetBrightnessValue(float Value);

	/** Function to set Local axes mode for the ELocalAxesType */
	void SetLocalAxesMode(ELocalAxesMode::Type AxesMode);

	/** Local axes mode checking function for the ELocalAxesType */
	bool IsLocalAxesModeSet(ELocalAxesMode::Type AxesMode) const;

	/** Get the Bone local axis mode */
	ELocalAxesMode::Type GetLocalAxesMode() const;

	/** Function to set Bone Draw  mode for the EBoneDrawType */
	void SetBoneDrawMode(EBoneDrawMode::Type AxesMode);

	/** Bone Draw  mode checking function for the EBoneDrawType */
	bool IsBoneDrawModeSet(EBoneDrawMode::Type AxesMode) const;

	/** Get the Bone local axis mode */
	EBoneDrawMode::Type GetBoneDrawMode() const;
	
	/** Returns the desired target of the camera */
	FSphere GetCameraTarget();

	/** Sets up the viewports camera (look-at etc) based on the current preview target*/
	void UpdateCameraSetup();

	/* Places the viewport camera at a good location to view the supplied sphere */
	void FocusViewportOnSphere( FSphere& Sphere, bool bInstant = true );

	/* Places the viewport camera at a good location to view the preview target */
	void FocusViewportOnPreviewMesh(bool bInstant = true);

	/** Callback for toggling the normals show flag. */
	void ToggleCPUSkinning();

	/** Callback for checking the normals show flag. */
	bool IsSetCPUSkinningChecked() const;


	/** Callback for toggling the normals show flag. */
	void ToggleShowNormals();

	/** Callback for checking the normals show flag. */
	bool IsSetShowNormalsChecked() const;

	/** Callback for toggling the tangents show flag. */
	void ToggleShowTangents();

	/** Callback for checking the tangents show flag. */
	bool IsSetShowTangentsChecked() const;

	/** Callback for toggling the binormals show flag. */
	void ToggleShowBinormals();

	/** Callback for checking the binormals show flag. */
	bool IsSetShowBinormalsChecked() const;

	/** Callback for toggling UV drawing in the viewport */
	void ToggleDrawUVOverlay();

	/** Callback for checking whether the UV drawing is switched on. */
	bool IsSetDrawUVOverlayChecked() const;

	/** Returns the UV Channel that will be drawn when Draw UV Overlay is turned on */
	int32 GetUVChannelToDraw() const { return UVChannelToDraw; }

	/** Sets the UV Channel that will be drawn when Draw UV Overlay is turned on */
	void SetUVChannelToDraw(int32 UVChannel) { UVChannelToDraw = UVChannel; }

	/* Returns the floor height offset */	
	float GetFloorOffset() const;

	/* Sets the floor height offset, saves it to config and invalidates the viewport so it shows up immediately */
	void SetFloorOffset(float NewValue);

	/** Function to set mesh stat drawing state */
	void OnSetShowMeshStats(int32 ShowMode);
	/** Whether or not mesh stats are being displayed */
	bool IsShowingMeshStats() const;
	/** Whether or not selected node stats are being displayed */
	bool IsShowingSelectedNodeStats() const;
	/** Whether detailed mesh stats are being displayed or basic mesh stats */
	bool IsDetailedMeshStats() const;

	int32 GetShowMeshStats() const;

	/** Set the playback speed mode */
	void SetPlaybackSpeedMode(EAnimationPlaybackSpeeds::Type InMode);

	/** Get the playback speed mode */
	EAnimationPlaybackSpeeds::Type GetPlaybackSpeedMode() const;

	/** Get the skeleton tree we are bound to */
	TSharedRef<class ISkeletonTree> GetSkeletonTree() const  { return SkeletonTreePtr.Pin().ToSharedRef(); }

	/** Get the preview scene we are viewing */
	TSharedRef<class IPersonaPreviewScene> GetPreviewScene() const { return PreviewScenePtr.Pin().ToSharedRef(); }

	/** Get the asset editor we are embedded in */
	TSharedRef<class FAssetEditorToolkit> GetAssetEditorToolkit() const { return AssetEditorToolkitPtr.Pin().ToSharedRef(); }

	/* Handle error checking for additive base pose */
	bool ShouldDisplayAdditiveScaleErrorMessage();

	/** Draws Mesh Sockets in foreground - bUseSkeletonSocketColor = true for grey (skeleton), false for red (mesh) **/
	static void DrawSockets(const UDebugSkelMeshComponent* InPreviewMeshComponent, TArray<USkeletalMeshSocket*>& InSockets, FSelectedSocketInfo InSelectedSocket, FPrimitiveDrawInterface* PDI, bool bUseSkeletonSocketColor);

	/** Draws Gizmo for the Transform in foreground **/
	static void RenderGizmo(const FTransform& Transform, FPrimitiveDrawInterface* PDI);

public:

	/** persona config options **/
	UPersonaOptions* ConfigOption;

private:
	/** Weak pointer back to the skeleton tree we are bound to */
	TWeakPtr<class ISkeletonTree> SkeletonTreePtr;

	/** Weak pointer back to the preview scene we are viewing */
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;

	/** Weak pointer back to asset editor we are embedded in */
	TWeakPtr<class FAssetEditorToolkit> AssetEditorToolkitPtr;

	// Current widget mode
	FWidget::EWidgetMode WidgetMode;

	/** add follow option @todo change to enum later - we share editorviewportclient, which is only problem*/
	bool bCameraFollow;

	/** Should we auto align floor to mesh bounds */
	bool bAutoAlignFloor;

	/** User selected color using color picker */
	FLinearColor SelectedHSVColor;

	/** Selected playback speed mode, used for deciding scale */
	EAnimationPlaybackSpeeds::Type AnimationPlaybackSpeedMode;

	/** Flag for displaying the UV data in the viewport */
	bool bDrawUVs;

	/** Which UV channel to draw */
	int32 UVChannelToDraw;

	enum GridParam
	{
		MinCellCount = 64,
		MinGridSize = 2,
		MaxGridSize	= 50,
	};

	/** Focus on the preview component the next time we draw the viewport */
	bool bFocusOnDraw;
	bool bInstantFocusOnDraw;

	/** Handle additive anim scale validation */
	bool bDoesAdditiveRefPoseHaveZeroScale;
	FGuid RefPoseGuid;

private:

	void SetSelectedBackgroundColor(const FLinearColor& RGBColor, bool bSave = true);

	void SetCameraTargetLocation(const FSphere &BoundSphere, float DeltaSeconds);

	/** Draws Mesh Bones in foreground **/
	void DrawMeshBones(USkeletalMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draws the given array of transforms as bones */
	void DrawBonesFromTransforms(TArray<FTransform>& Transforms, UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI, FLinearColor BoneColour, FLinearColor RootBoneColour) const;
	/** Draws Bones for a compact pose */
	void DrawBonesFromCompactPose(const FCompactHeapPose& Pose, UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI, const FLinearColor& DrawColour) const;
	/** Draws Bones for uncompressed animation **/
	void DrawMeshBonesUncompressedAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draw Bones for non retargeted animation. */
	void DrawMeshBonesNonRetargetedAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draws Bones for Additive Base Pose */
	void DrawMeshBonesAdditiveBasePose(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draw Bones for non retargeted animation. */
	void DrawMeshBonesSourceRawAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draw Bones for non retargeted animation. */
	void DrawMeshBonesBakedAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draws Bones for RequiredBones with WorldTransform **/
	void DrawBones(const USkeletalMeshComponent* MeshComponent, const TArray<FBoneIndexType> & RequiredBones, const TArray<FTransform> & WorldTransforms, FPrimitiveDrawInterface* PDI, const TArray<FLinearColor>& BoneColours, float LineThickness = 0.f, bool bForceDraw = false) const;
	/** Draw Sub set of Bones **/
	void DrawMeshSubsetBones(const USkeletalMeshComponent* MeshComponent, const TArray<int32>& BonesOfInterest, FPrimitiveDrawInterface* PDI) const;

	/** Draws bones from watched poses*/
	void DrawWatchedPoses(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI);

	/** Get the typed anim preview scene */
	TSharedRef<class FAnimationEditorPreviewScene> GetAnimPreviewScene() const;

	/** Get the persona mode manager */
	class IPersonaEditorModeManager& GetPersonaModeManager() const;

	/** Invalidate this view in response to a preview scene change */
	void HandleInvalidateViews();

	/** Delegate for preview profile is changed (used for updating show flags) */
	void OnAssetViewerSettingsChanged(const FName& InPropertyName);

	/** Sets up the ShowFlag according to the current preview scene profile */
	void SetAdvancedShowFlagsForScene();

private:
	/** Allow mesh stats to be disabled for specific viewport instances */
	bool bShowMeshStats;
};
