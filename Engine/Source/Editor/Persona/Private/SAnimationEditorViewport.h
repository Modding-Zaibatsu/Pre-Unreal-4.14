// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "SEditorViewport.h"
#include "IPersonaViewport.h"

struct FAnimationEditorViewportRequiredArgs
{
	FAnimationEditorViewportRequiredArgs(const TSharedRef<class ISkeletonTree>& InSkeletonTree, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, TSharedRef<class SAnimationEditorViewportTabBody> InTabBody, TSharedRef<class FAssetEditorToolkit> InAssetEditorToolkit, FSimpleMulticastDelegate& InOnPostUndo)
		: SkeletonTree(InSkeletonTree)
		, PreviewScene(InPreviewScene)
		, TabBody(InTabBody)
		, AssetEditorToolkit(InAssetEditorToolkit)
		, OnPostUndo(InOnPostUndo)
	{}

	TSharedRef<class ISkeletonTree> SkeletonTree;

	TSharedRef<class IPersonaPreviewScene> PreviewScene;

	TSharedRef<class SAnimationEditorViewportTabBody> TabBody;

	TSharedRef<class FAssetEditorToolkit> AssetEditorToolkit;

	FSimpleMulticastDelegate& OnPostUndo;
};

//////////////////////////////////////////////////////////////////////////
// SAnimationEditorViewport

class SAnimationEditorViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SAnimationEditorViewport) 
		: _ShowStats(true)
	{}

	SLATE_ARGUMENT(bool, ShowStats)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAnimationEditorViewportRequiredArgs& InRequiredArgs);

protected:
	// SEditorViewport interface
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	// End of SEditorViewport interface

	/**  Handle undo/redo by refreshing the viewport */
	void OnUndoRedo();

protected:
	// Viewport client
	TSharedPtr<class FAnimationViewportClient> LevelViewportClient;

	// Pointer to the compound widget that owns this viewport widget
	TWeakPtr<class SAnimationEditorViewportTabBody> TabBodyPtr;

	// The preview scene that we are viewing
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;

	// The skeleton tree we are editing
	TWeakPtr<class ISkeletonTree> SkeletonTreePtr;

	// The asset editor we are embedded in
	TWeakPtr<class FAssetEditorToolkit> AssetEditorToolkitPtr;

	/** Whether we should show stats for this viewport */
	bool bShowStats;
};

//////////////////////////////////////////////////////////////////////////
// SAnimationEditorViewportTabBody

class SAnimationEditorViewportTabBody : public IPersonaViewport
{
public:
	SLATE_BEGIN_ARGS( SAnimationEditorViewportTabBody )
		: _BlueprintEditor()
		, _ShowTimeline(true)
		{}

		SLATE_ARGUMENT(TWeakPtr<FBlueprintEditor>, BlueprintEditor)

		SLATE_ARGUMENT(FOnInvokeTab, OnInvokeTab)

		SLATE_ARGUMENT(bool, ShowTimeline)

		SLATE_ARGUMENT(bool, ShowStats)
	SLATE_END_ARGS()
public:

	void Construct(const FArguments& InArgs, const TSharedRef<class ISkeletonTree>& InSkeletonTree, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, const TSharedRef<class FAssetEditorToolkit>& InAssetEditorToolkit, FSimpleMulticastDelegate& InOnUndoRedo);
	SAnimationEditorViewportTabBody();
	virtual ~SAnimationEditorViewportTabBody();

	/** IPersonaViewport interface */
	virtual TSharedRef<IPersonaViewportState> SaveState() const override;
	virtual void RestoreState(TSharedRef<IPersonaViewportState> InState) override;
	
	void RefreshViewport();

	/**
	 * @return The list of commands on the viewport that are bound to delegates                    
	 */
	const TSharedPtr<FUICommandList>& GetCommandList() const { return UICommandList; }

	/** Handle the skeletal mesh changing */
	void HandlePreviewMeshChanged(class USkeletalMesh* SkeletalMesh);

	/** Function to get the number of LOD models associated with the preview skeletal mesh*/
	int32 GetLODModelCount() const;

	/** LOD model selection checking function*/
	bool IsLODModelSelected( int32 LODSelectionType ) const;
	int32 GetLODSelection() const { return LODSelection; };

	/** Function to set the current playback speed*/
	void OnSetPlaybackSpeed(int32 PlaybackSpeedMode);

	/** Function to return whether the supplied playback speed is the current active one */
	bool IsPlaybackSpeedSelected(int32 PlaybackSpeedMode);

	/** Function to get anim viewport widget */
	TSharedPtr<class SEditorViewport> GetViewportWidget() const { return ViewportWidget; }

	/** Function to get viewport's current background color */
	FLinearColor GetViewportBackgroundColor( ) const;

	/** Function to set viewport's new background color */
	void SetViewportBackgroundColor( FLinearColor InColor );

	/** Function to get viewport's background color brightness */
	float GetBackgroundBrightness( ) const;

	/** Function to set viewport's background color brightness */
	void SetBackgroundBrightness( float Value );

	/** Function to check whether grid is displayed or not */
	bool IsShowingGrid() const;

	/** Gets the editor client for this viewport */
	FEditorViewportClient& GetLevelViewportClient()
	{		
		return *LevelViewportClient;
	}

	/** Gets the animation viewport client */
	TSharedRef<class FAnimationViewportClient> GetAnimationViewportClient() const;

	/** Returns Detail description of what's going with viewport **/
	FText GetDisplayString() const;

	/** Can we use gizmos? */
	bool CanUseGizmos() const;

	/** Function to check whether floor is auto aligned or not */
	bool IsAutoAlignFloor() const;

	void SetWindStrength( float SliderPos );

	/** Function to get slider value which represents wind strength (0 - 1)*/
	float GetWindStrengthSliderValue() const;

	/** Function to get slider value which returns a string*/
	FText GetWindStrengthLabel() const;

	bool IsApplyingClothWind() const;

	/** Show gravity scale */
	void SetGravityScale( float SliderPos );
	float GetGravityScaleSliderValue() const;
	FText GetGravityScaleLabel() const;

	/** Function to set LOD model selection*/
	void OnSetLODModel(int32 LODSelectionType);

	/** Get the skeleton tree we are bound to */
	TSharedRef<class ISkeletonTree> GetSkeletonTree() const { return SkeletonTreePtr.Pin().ToSharedRef(); }

	/** Get the preview scene we are viewing */
	TSharedRef<class FAnimationEditorPreviewScene> GetPreviewScene() const { return PreviewScenePtr.Pin().ToSharedRef(); }

	/** Get the skeleton tree we are bound to */
	TSharedPtr<class FAssetEditorToolkit> GetAssetEditorToolkit() const { return AssetEditorToolkitPtr.Pin(); }

protected:


private:
	bool IsVisible() const;

	/**
	 * Binds our UI commands to delegates
	 */ 
	void BindCommands();

	/** Show Morphtarget of SkeletalMesh **/
	void OnShowMorphTargets();

	bool IsShowingMorphTargets() const;

	/** Show Raw Animation on top of Compressed Animation **/
	void OnShowRawAnimation();

	bool IsShowingRawAnimation() const;

	/** Show non retargeted animation. */
	void OnShowNonRetargetedAnimation();

	bool IsShowingNonRetargetedPose() const;

	/** Show non retargeted animation. */
	void OnShowSourceRawAnimation();

	bool IsShowingSourceRawAnimation() const;

	/** Show non retargeted animation. */
	void OnShowBakedAnimation();

	bool IsShowingBakedAnimation() const;


	/** Additive Base Pose on top of full animation **/
	void OnShowAdditiveBase();

	bool IsShowingAdditiveBase() const;

	bool IsPreviewingAnimation() const;

	/** Function to show/hide bone names */
	void OnShowBoneNames();

	/** Function to check whether bone names are displayed or not */
	bool IsShowingBoneNames() const;
	
	/** Function to show/hide selected bone weight */
	void OnShowOverlayNone();

	/** Function to check whether bone weights are displayed or not*/
	bool IsShowingOverlayNone() const;

	/** Function to show/hide selected bone weight */
	void OnShowOverlayBoneWeight();

	/** Function to check whether bone weights are displayed or not*/
	bool IsShowingOverlayBoneWeight() const;

	/** Function to show/hide selected morphtarget overlay*/
	void OnShowOverlayMorphTargetVert();

	/** Function to check whether morphtarget overlay is displayed or not*/
	bool IsShowingOverlayMorphTargetVerts() const;

	/** Function to set Local axes mode of the specificed type */
	void OnSetBoneDrawMode(int32 BoneDrawMode);

	/** Local axes mode checking function for the specificed type*/
	bool IsBoneDrawModeSet(int32 BoneDrawMode) const;

	/** Function to set Local axes mode of the specificed type */
	void OnSetLocalAxesMode( int32 LocalAxesMode );

	/** Local axes mode checking function for the specificed type*/
	bool IsLocalAxesModeSet( int32 LocalAxesMode ) const;

	/** Function to show/hide socket hit points */
	void OnShowSockets();

	/** Function to check whether socket hit points are displayed or not*/
	bool IsShowingSockets() const;

	/** Function to show/hide mesh info*/
	void OnShowDisplayInfo(int32 DisplayInfoMode);

	/** Function to check whether mesh info is displayed or not */
	bool IsShowingMeshInfo(int32 DisplayInfoMode) const;

	/** Function to show/hide grid in the viewport */
	void OnShowGrid();	

	/** Toggles floor alignment in the preview scene */
	void OnToggleAutoAlignFloor();

	/** Called to toggle showing of reference pose on current preview mesh */
	void ShowRetargetBasePose();
	bool CanShowRetargetBasePose() const;
	bool IsShowRetargetBasePoseEnabled() const;

	/** Called to toggle showing of the bounds of the current preview mesh */
	void ShowBound();
	bool CanShowBound() const;
	bool IsShowBoundEnabled() const;

	/** Called to toggle showing of the current preview mesh */
	void ToggleShowPreviewMesh();
	bool CanShowPreviewMesh() const;
	bool IsShowPreviewMeshEnabled() const;

	/** Called to toggle using in-game bound on current preview mesh */
	void UseInGameBound();
	bool CanUseInGameBound() const;
	bool IsUsingInGameBound() const;

	/** Called by UV channel combo box on selection change */
	void ComboBoxSelectionChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo );

	/** Populates choices for UV Channel combo box for each lod based on current preview asset */
	void PopulateNumUVChannels();

	/** Populates choices for UV Channel combo box */
	void PopulateUVChoices();

	void AnimChanged(UAnimationAsset* AnimAsset);

	/** Open the preview scene settings */
	void OpenPreviewSceneSettings();

	/** Called to toggle camera lock for naviagating **/
	void ToggleCameraFollow();
	bool IsCameraFollowEnabled() const;

	/** Called to determine whether the camera mode menu options should be enabled */
	bool CanChangeCameraMode() const;

	/** Tests to see if bone move mode buttons should be visibile */
	EVisibility GetBoneMoveModeButtonVisibility() const;

	/** Function to mute/unmute viewport audio */
	void OnMuteAudio();

	/** Whether audio from the viewport is muted */
	bool IsAudioMuted();

	/** Function to set whether we are previewing root motion */
	void OnTogglePreviewRootMotion();
	
	/** Whether or not we are previewing root motion */
	bool IsPreviewingRootMotion() const;

private:
	/** Selected Turn Table speed  */
	EAnimationPlaybackSpeeds::Type SelectedTurnTableSpeed;
	/** Selected turn table mode */
	EPersonaTurnTableMode::Type SelectedTurnTableMode;

	void OnSetTurnTableSpeed(int32 SpeedIndex);
	void OnSetTurnTableMode(int32 ModeIndex);
	bool IsTurnTableModeSelected(int32 ModeIndex) const;

public:
	bool IsTurnTableSpeedSelected(int32 SpeedIndex) const;

#if WITH_APEX_CLOTHING
	/** 
	 * clothing show options 
	*/
private:
	/** disable cloth simulation */
	void OnDisableClothSimulation();
	bool IsDisablingClothSimulation() const;

	void OnApplyClothWind();

	/** Show cloth simulation normals */
	void OnShowClothSimulationNormals();
	bool IsShowingClothSimulationNormals() const;

	/** Show cloth graphical tangents */
	void OnShowClothGraphicalTangents();
	bool IsShowingClothGraphicalTangents() const;

	/** Show cloth collision volumes */
	void OnShowClothCollisionVolumes();
	bool IsShowingClothCollisionVolumes() const;

	/** Enable collision with clothes on attached children */
	void OnEnableCollisionWithAttachedClothChildren();
	bool IsEnablingCollisionWithAttachedClothChildren() const;

	/** Show cloth physical mesh wire */
	void OnShowClothPhysicalMeshWire();
	bool IsShowingClothPhysicalMeshWire() const;

	/** Show cloth max distances */
	void OnShowClothMaxDistances();
	bool IsShowingClothMaxDistances() const;

	/** Show cloth back stops */
	void OnShowClothBackstops();
	bool IsShowingClothBackstops() const;

	/** Show only fixed vertices */
	void OnShowClothFixedVertices();
	bool IsShowingClothFixedVertices() const;

	/** Show all sections which means the original state */
	void OnSetSectionsDisplayMode(int32 DisplayMode);
	bool IsSectionsDisplayMode(int32 DisplayMode) const;

#endif // #if WITH_APEX_CLOTHING

private:
	/** Weak pointer back to the skeleton tree we are bound to */
	TWeakPtr<class ISkeletonTree> SkeletonTreePtr;

	/** Weak pointer back to the preview scene we are viewing */
	TWeakPtr<class FAnimationEditorPreviewScene> PreviewScenePtr;

	/** Weak pointer back to asset editor we are embedded in */
	TWeakPtr<class FAssetEditorToolkit> AssetEditorToolkitPtr;

	/** Weak pointer to the blueprint editor we are optionally embedded in */
	TWeakPtr<class FBlueprintEditor> BlueprintEditorPtr;

	/** Whether to show the timeline */
	bool bShowTimeline;

	/** Level viewport client */
	TSharedPtr<FEditorViewportClient> LevelViewportClient;

	/** Viewport widget*/
	TSharedPtr<class SAnimationEditorViewport> ViewportWidget;

	/** Toolbar widget */
	TSharedPtr<SHorizontalBox> ToolbarBox;

	/** Commands that are bound to delegates*/
	TSharedPtr<FUICommandList> UICommandList;

	/** Delegate used to invoke tabs in the containing asset editor */
	FOnInvokeTab OnInvokeTab;

public:
	/** UV Channel Selector */
	TSharedPtr< class STextComboBox > UVChannelCombo;
	
private:
	/** Choices for UVChannelCombo */
	TArray< TSharedPtr< FString > > UVChannels;

	/** Num UV Channels at each LOD of Preview Mesh */
	TArray<int32> NumUVChannels;

	/** Box that contains scrub panel */
	TSharedPtr<SVerticalBox> ScrubPanelContainer;

	/** Current LOD selection*/
	int32 LODSelection;

	/** Get Min/Max Input of value **/
	float GetViewMinInput() const;
	float GetViewMaxInput() const;

	/** Sets The EngineShowFlags.MeshEdges flag on the viewport based on current state */
	void UpdateShowFlagForMeshEdges();

	/** Update scrub panel to reflect viewed animation asset */
	void UpdateScrubPanel(UAnimationAsset* AnimAsset);
private:
	friend class FPersona;

	EVisibility GetViewportCornerTextVisibility() const;
	FText GetViewportCornerText() const;
	FText GetViewportCornerTooltip() const;
	FReply ClickedOnViewportCornerText();

};
