// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDocumentation.h"
#include "AnimationEditorViewportClient.h"
#include "IPersonaViewport.h"
#include "PersonaModule.h"
#include "SSingleObjectDetailsPanel.h"

#define LOCTEXT_NAMESPACE "PersonaMode"

/////////////////////////////////////////////////////

struct FPersonaTabs
{
	// Tab constants

	// Selection Details
	static const FName MorphTargetsID;
	static const FName AnimCurveViewID;
	static const FName SkeletonTreeViewID;
	// Skeleton Pose manager
	static const FName RetargetManagerID;
	static const FName RigManagerID;
	// Skeleton/Sockets
	// Anim Blueprint Params
	// Explorer
	// Class Defaults
	static const FName AnimBlueprintPreviewEditorID;
	static const FName AnimBlueprintParentPlayerEditorID;
	// Anim Document
	static const FName ScrubberID;
	// Toolbar
	static const FName PreviewViewportID;
	static const FName AssetBrowserID;
	static const FName MirrorSetupID;
	static const FName AnimBlueprintDebugHistoryID;
	static const FName AnimAssetPropertiesID;
	static const FName MeshAssetPropertiesID;
	static const FName PreviewManagerID;
	static const FName SkeletonAnimNotifiesID;
	static const FName SkeletonSlotNamesID;
	static const FName SkeletonSlotGroupNamesID;
	static const FName CurveNameManagerID;
	static const FName BlendProfileManagerID;

	// Advanced Preview Scene
	static const FName AdvancedPreviewSceneSettingsID;

	static const FName DetailsID;

	// Blueprint Document

	// Inherited from blueprint editor
	/*
	static const FName Toolbar;
	static const FName Explorer;
	static const FName VariableList;
	static const FName Inspector;
	etc...
	*/

private:
	FPersonaTabs() {}
};

/////////////////////////////////////////////////////

// This is the list of IDs for persona modes
struct FPersonaModes
{
	// Mode constants
	static const FName SkeletonDisplayMode;
	static const FName MeshEditMode;
	static const FName PhysicsEditMode;
	static const FName AnimationEditMode;
	static const FName AnimBlueprintEditMode;
	static FText GetLocalizedMode( const FName InMode )
	{
		static TMap< FName, FText > LocModes;

		if (LocModes.Num() == 0)
		{
			LocModes.Add( SkeletonDisplayMode, NSLOCTEXT("PersonaModes", "SkeletonDisplayMode", "Skeleton") );
			LocModes.Add( MeshEditMode, NSLOCTEXT("PersonaModes", "MeshEditMode", "Mesh") );
			LocModes.Add( PhysicsEditMode, NSLOCTEXT("PersonaModes", "PhysicsEditMode", "Physics") );
			LocModes.Add( AnimationEditMode, NSLOCTEXT("PersonaModes", "AnimationEditMode", "Animation") );
			LocModes.Add( AnimBlueprintEditMode, NSLOCTEXT("PersonaModes", "AnimBlueprintEditMode", "Graph") );
		}

		check( InMode != NAME_None );
		const FText* OutDesc = LocModes.Find( InMode );
		check( OutDesc );
		return *OutDesc;
	}
private:
	FPersonaModes() {}
};

/////////////////////////////////////////////////////
// FPersonaModeSharedData

struct FPersonaModeSharedData : public IPersonaViewportState
{
	FPersonaModeSharedData();

	void Save(const TSharedRef<FAnimationViewportClient>& InFromViewport);
	void Restore(const TSharedRef<FAnimationViewportClient>& InToViewport);

	// camera setup
	FVector				ViewLocation;
	FRotator			ViewRotation;
	float				OrthoZoom;
	
	// orbit setup
	FVector				OrbitZoom;
	FVector				LookAtLocation;
	bool				bCameraLock;
	bool				bCameraFollow;

	// show flags
	bool				bShowReferencePose;
	bool				bShowBones;
	bool				bShowBoneNames;
	bool				bShowSockets;
	bool				bShowBound;

	// viewport setup
	int32				ViewportType;
	EAnimationPlaybackSpeeds::Type PlaybackSpeedMode;
	int32				LocalAxesMode;
};

/////////////////////////////////////////////////////
// FPersonaAppMode

class FPersonaAppMode : public FApplicationMode
{
protected:
	FPersonaAppMode(TSharedPtr<class FPersona> InPersona, FName InModeName);

public:
	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	virtual void PostActivateMode() override;
	// End of FApplicationMode interface

protected:
	TWeakPtr<class FPersona> MyPersona;

	// Set of spawnable tabs in persona mode (@TODO: Multiple lists!)
	FWorkflowAllowedTabSet PersonaTabFactories;
};

/////////////////////////////////////////////////////
// FSkeletonTreeSummoner

struct FSkeletonTreeSummoner : public FWorkflowTabFactory
{
public:
	FSkeletonTreeSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("SkeletonTreeTooltip", "The Skeleton Tree tab lets you see and select bones (and sockets) in the skeleton hierarchy."), NULL, TEXT("Shared/Editors/Persona"), TEXT("SkeletonTree_Window"));
	}
};

/////////////////////////////////////////////////////
// FMorphTargetTabSummoner

struct FMorphTargetTabSummoner : public FWorkflowTabFactory
{
public:
	FMorphTargetTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("MorphTargetTooltip", "The Morph Target tab lets you preview any morph targets (aka blend shapes) available for the current mesh."), NULL, TEXT("Shared/Editors/Persona"), TEXT("MorphTarget_Window"));
	}

private:
	TWeakPtr<class IPersonaPreviewScene> PreviewScene;
	FSimpleMulticastDelegate& OnPostUndo;
};

/////////////////////////////////////////////////////
// FAnimCurveViewerTabSummoner

struct FAnimCurveViewerTabSummoner : public FWorkflowTabFactory
{
public:
	FAnimCurveViewerTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnCurvesChanged, FSimpleMulticastDelegate& InOnPostUndo);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("AnimCurveViewTooltip", "The Anim Curve Viewer tab lets you preview any animation curves available for the current mesh from preview asset."), NULL, TEXT("Shared/Editors/Persona"), TEXT("AnimCurveView_Window"));
	}

private:
	TWeakPtr<class IEditableSkeleton> EditableSkeleton;
	TWeakPtr<class IPersonaPreviewScene> PreviewScene;
	FSimpleMulticastDelegate& OnCurvesChanged;
	FSimpleMulticastDelegate& OnPostUndo;
};


/////////////////////////////////////////////////////
// FAnimationAssetBrowserSummoner

struct FAnimationAssetBrowserSummoner : public FWorkflowTabFactory
{
	FAnimationAssetBrowserSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<IPersonaToolkit>& InPersonaToolkit, FOnOpenNewAsset InOnOpenNewAsset, FOnAnimationSequenceBrowserCreated InOnAnimationSequenceBrowserCreated, bool bInShowHistory);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("AnimAssetBrowserTooltip", "The Asset Browser lets you browse all animation-related assets (animations, blend spaces etc)."), NULL, TEXT("Shared/Editors/Persona"), TEXT("AssetBrowser_Window"));
	}

private:
	TWeakPtr<class IPersonaToolkit> PersonaToolkit;
	FOnOpenNewAsset OnOpenNewAsset;
	FOnAnimationSequenceBrowserCreated OnAnimationSequenceBrowserCreated;
	bool bShowHistory;
};

/////////////////////////////////////////////////////
// FPreviewViewportSummoner

struct FPreviewViewportSummoner : public FWorkflowTabFactory
{
	FPreviewViewportSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<ISkeletonTree>& InSkeletonTree, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo, const TSharedPtr<FBlueprintEditor>& InBlueprintEditor, FOnViewportCreated InOnViewportCreated, bool bShowTimeline, bool bInShowStats);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	TWeakPtr<ISkeletonTree> SkeletonTree;
	TWeakPtr<IPersonaPreviewScene> PreviewScene;
	FSimpleMulticastDelegate& OnPostUndo;
	TWeakPtr<FBlueprintEditor> BlueprintEditor;
	FOnViewportCreated OnViewportCreated;
	bool bShowTimeline;
	bool bShowStats;
};

/////////////////////////////////////////////////////
// FRetargetManagerTabSummoner

struct FRetargetManagerTabSummoner : public FWorkflowTabFactory
{
public:
	FRetargetManagerTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("RetargetSourceTooltip", "In this panel, you can manage retarget sources for different body types"), NULL, TEXT("Shared/Editors/Persona"), TEXT("RetargetManager"));
	}

private:
	TWeakPtr<class IEditableSkeleton> EditableSkeleton;
	TWeakPtr<class IPersonaPreviewScene> PreviewScene;
	FSimpleMulticastDelegate& OnPostUndo;
};

/////////////////////////////////////////////////////
// SPersonaPreviewPropertyEditor

class SPersonaPreviewPropertyEditor : public SSingleObjectDetailsPanel
{
public:
	SLATE_BEGIN_ARGS(SPersonaPreviewPropertyEditor) {}
	SLATE_END_ARGS()

private:
	// Pointer to preview scene
	TWeakPtr<IPersonaPreviewScene> PreviewScene;

public:
	void Construct(const FArguments& InArgs, TSharedRef<IPersonaPreviewScene> InPreviewScene);

	// SSingleObjectDetailsPanel interface
	virtual UObject* GetObjectToObserve() const override;
	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget) override;
	// End of SSingleObjectDetailsPanel interface

private:
	void HandlePropertyChanged(const FPropertyChangedEvent& InPropertyChangedEvent);
	FReply HandleApplyChanges();

private:
	bool bPropertyEdited;
};

/////////////////////////////////////////////////////
// FAnimBlueprintPreviewEditorSummoner

namespace EAnimBlueprintEditorMode
{
	enum Type
	{
		PreviewMode,
		DefaultsMode
	};
}

struct FAnimBlueprintPreviewEditorSummoner : public FWorkflowTabFactory
{
public:
	FAnimBlueprintPreviewEditorSummoner(TSharedPtr<class FBlueprintEditor> InBlueprintEditor, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

private:
	/** Delegates to customize tab look based on selected mode */
	EVisibility IsEditorVisible(EAnimBlueprintEditorMode::Type Mode) const;
	ECheckBoxState IsChecked(EAnimBlueprintEditorMode::Type Mode) const;
	const FSlateBrush* GetBorderBrushByMode(EAnimBlueprintEditorMode::Type Mode) const;

	/** Handle changing of editor mode */
	void OnCheckedChanged(ECheckBoxState NewType, EAnimBlueprintEditorMode::Type Mode);
	EAnimBlueprintEditorMode::Type CurrentMode;

	TWeakPtr<class FBlueprintEditor> BlueprintEditor;
	TWeakPtr<class IPersonaPreviewScene> PreviewScene;
};

//////////////////////////////////////////////////////////////////////////
// FAnimBlueprintParentPlayerEditorSummoner
class FAnimBlueprintParentPlayerEditorSummoner : public FWorkflowTabFactory
{
public:
	FAnimBlueprintParentPlayerEditorSummoner(TSharedPtr<class FBlueprintEditor> InBlueprintEditor, FSimpleMulticastDelegate& InOnPostUndo);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

private:
	TWeakPtr<class FBlueprintEditor> BlueprintEditor;
	FSimpleMulticastDelegate& OnPostUndo;
};

/////////////////////////////////////////////////////
// FAdvancedPreviewSceneTabSummoner

struct FAdvancedPreviewSceneTabSummoner : public FWorkflowTabFactory
{
public:
 	FAdvancedPreviewSceneTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene);
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

private:
	TWeakPtr<class IPersonaPreviewScene> PreviewScene;
};

/////////////////////////////////////////////////////
// FDetailsTabSummoner

struct FDetailsTabSummoner : public FWorkflowTabFactory
{
public:
	FDetailsTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, FOnDetailsCreated InOnDetailsCreated);
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

private:
	FOnDetailsCreated OnDetailsCreated;
	TSharedPtr<class SPersonaDetails> PersonaDetails;
};

#undef LOCTEXT_NAMESPACE