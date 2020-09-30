// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkit.h"		// For EToolkitMode::Type
#include "Toolkits/AssetEditorToolkit.h" 
#include "ModuleInterface.h"
#include "PersonaDelegates.h"
#include "Editor.h"
#include "Factories/FbxImportUI.h"

extern const FName PersonaAppName;

// Editor mode constants
struct PERSONA_API FPersonaEditModes
{
	/** Selection/manipulation of bones & sockets */
	const static FEditorModeID SkeletonSelection;
};

DECLARE_DELEGATE_TwoParams(FIsRecordingActive, USkeletalMeshComponent* /*Component*/, bool& /* bIsRecording */);
DECLARE_DELEGATE_OneParam(FRecord, USkeletalMeshComponent* /*Component*/);
DECLARE_DELEGATE_OneParam(FStopRecording, USkeletalMeshComponent* /*Component*/);
DECLARE_DELEGATE_TwoParams(FGetCurrentRecording, USkeletalMeshComponent* /*Component*/, class UAnimSequence*& /* OutRecording */);
DECLARE_DELEGATE_TwoParams(FGetCurrentRecordingTime, USkeletalMeshComponent* /*Component*/, float& /* OutTime */);
DECLARE_DELEGATE_TwoParams(FTickRecording, USkeletalMeshComponent* /*Component*/, float /* DeltaSeconds */);

/** Called back when a viewport is created */
DECLARE_DELEGATE_OneParam(FOnViewportCreated, const TSharedRef<class IPersonaViewport>&);

/** Called back when a details panel is created */
DECLARE_DELEGATE_OneParam(FOnDetailsCreated, const TSharedRef<class IDetailsView>&);

/** Called back when an anim sequence browser is created */
DECLARE_DELEGATE_OneParam(FOnAnimationSequenceBrowserCreated, const TSharedRef<class IAnimationSequenceBrowser>&);

/** Initialization parameters for persona toolkits */
struct FPersonaToolkitArgs
{
	/** Whether to create a preview scene */
	bool bCreatePreviewScene;

	FPersonaToolkitArgs()
		: bCreatePreviewScene(true)
	{}
};

struct FAnimDocumentArgs
{
	FAnimDocumentArgs(const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, const TSharedRef<class IPersonaToolkit>& InPersonaToolkit, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnPostUndo, FSimpleMulticastDelegate& InOnCurvesChanged, FSimpleMulticastDelegate& InOnAnimNotifiesChanged, FSimpleMulticastDelegate& InOnSectionsChanged)
		: PreviewScene(InPreviewScene)
		, PersonaToolkit(InPersonaToolkit)
		, EditableSkeleton(InEditableSkeleton)
		, OnPostUndo(InOnPostUndo)
		, OnCurvesChanged(InOnCurvesChanged)
		, OnAnimNotifiesChanged(InOnAnimNotifiesChanged)
		, OnSectionsChanged(InOnSectionsChanged)
	{}

	/** Required args */
	TWeakPtr<class IPersonaPreviewScene> PreviewScene;
	TWeakPtr<class IPersonaToolkit> PersonaToolkit;
	TWeakPtr<class IEditableSkeleton> EditableSkeleton;
	FSimpleMulticastDelegate& OnPostUndo;
	FSimpleMulticastDelegate& OnCurvesChanged;
	FSimpleMulticastDelegate& OnAnimNotifiesChanged;
	FSimpleMulticastDelegate& OnSectionsChanged;

	/** Optional args */
	FOnObjectsSelected OnDespatchObjectsSelected;
	FOnInvokeTab OnDespatchInvokeTab;
	FSimpleDelegate OnDespatchCurvesChanged;
	FSimpleDelegate OnDespatchSectionsChanged;
	FSimpleDelegate OnDespatchAnimNotifiesChanged;
};

/**
 * Persona module manages the lifetime of all instances of Persona editors.
 */
class FPersonaModule : public IModuleInterface,
	public IHasMenuExtensibility
{
public:
	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule();

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule();

	/**
	 * Creates an instance of a Persona editor.  Only virtual so that it can be called across the DLL boundary.
	 *
	 * Note: This function should not be called directly, use one of the following instead:
	 *	- FKismetEditorUtilities::BringKismetToFocusAttentionOnObject
	 *  - FAssetEditorManager::Get().OpenEditorForAsset
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	AnimBlueprint			The blueprint object to start editing.  If specified, Skeleton and AnimationAsset must be NULL.
	 * @param	Skeleton				The skeleton to edit.  If specified, Blueprint must be NULL.
	 * @param	AnimationAsset			The animation asset to edit.  If specified, Blueprint must be NULL.
	 *
	 * @return	Interface to the new Persona editor
	 */
	virtual TSharedRef<class IBlueprintEditor> CreatePersona( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, class USkeleton* Skeleton, class UAnimBlueprint* Blueprint, class UAnimationAsset* AnimationAsset, class USkeletalMesh * Mesh );

	/** Create a re-usable toolkit that multiple asset editors that are concerned with USkeleton-related data can use */
	virtual TSharedRef<class IPersonaToolkit> CreatePersonaToolkit(USkeleton* InSkeleton, const FPersonaToolkitArgs& PersonaToolkitArgs = FPersonaToolkitArgs()) const;
	virtual TSharedRef<class IPersonaToolkit> CreatePersonaToolkit(UAnimationAsset* InAnimationAsset, const FPersonaToolkitArgs& PersonaToolkitArgs = FPersonaToolkitArgs()) const;
	virtual TSharedRef<class IPersonaToolkit> CreatePersonaToolkit(USkeletalMesh* InSkeletalMesh, const FPersonaToolkitArgs& PersonaToolkitArgs = FPersonaToolkitArgs()) const;
	virtual TSharedRef<class IPersonaToolkit> CreatePersonaToolkit(UAnimBlueprint* InAnimBlueprint, const FPersonaToolkitArgs& PersonaToolkitArgs = FPersonaToolkitArgs()) const;

	/** Create an asset family for the supplied persona asset */
	virtual TSharedRef<class IAssetFamily> CreatePersonaAssetFamily(const UObject* InAsset) const;

	/** Create a shortcut widget for an asset family */
	virtual TSharedRef<SWidget> CreateAssetFamilyShortcutWidget(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IAssetFamily>& InAssetFamily) const;

	/** Create a details panel tab factory */
	virtual TSharedRef<class FWorkflowTabFactory> CreateDetailsTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, FOnDetailsCreated InOnDetailsCreated) const;

	/** Create a persona viewport tab factory */
	virtual TSharedRef<class FWorkflowTabFactory> CreatePersonaViewportTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class ISkeletonTree>& InSkeletonTree, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& OnPostUndo, const TSharedPtr<class FBlueprintEditor>& InBlueprintEditor, FOnViewportCreated InOnViewportCreated, bool bInShowTimeline, bool bInShowStats) const;

	/** Create an anim notifies tab factory */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAnimNotifiesTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnChangeAnimNotifies, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectsSelected InOnObjectsSelected) const;

	/** Create a skeleton cuve viewer tab factory */
	virtual TSharedRef<class FWorkflowTabFactory> CreateCurveViewerTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnCurvesChanged, FSimpleMulticastDelegate& InOnPostUndo) const;

	/** Create a retarget manager tab factory */
	virtual TSharedRef<class FWorkflowTabFactory> CreateRetargetManagerTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo) const;

	/** Create a tab factory used to configure preview scene settings */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAdvancedPreviewSceneTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene) const;

	/** Create a tab factory for the animation asset browser */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAnimationAssetBrowserTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaToolkit>& InPersonaToolkit, FOnOpenNewAsset InOnOpenNewAsset, FOnAnimationSequenceBrowserCreated InOnAnimationSequenceBrowserCreated, bool bInShowHistory) const;

	/** Create a tab factory for editing a single object (like an animation asset) */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAssetDetailsTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, FOnGetAsset InOnGetAsset, FOnDetailsCreated InOnDetailsCreated) const;

	/** Create a tab factory for for previewing morph targets */
	virtual TSharedRef<class FWorkflowTabFactory> CreateMorphTargetTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& OnPostUndo) const;

	/** Create a tab factory for editing anim blueprint preview & defaults */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAnimBlueprintPreviewTabFactory(const TSharedRef<class FBlueprintEditor>& InBlueprintEditor, const TSharedRef<IPersonaPreviewScene>& InPreviewScene) const;

	/** Create a tab factory for editing anim blueprint parent overrides */
	virtual TSharedRef<class FWorkflowTabFactory> CreateAnimBlueprintAssetOverridesTabFactory(const TSharedRef<class FBlueprintEditor>& InBlueprintEditor, UAnimBlueprint* InAnimBlueprint, FSimpleMulticastDelegate& InOnPostUndo) const;

	/** Create a tab factory for editing slot names and groups */
	virtual TSharedRef<class FWorkflowTabFactory> CreateSkeletonSlotNamesTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, FSimpleMulticastDelegate& InOnPostUndo, FOnObjectSelected InOnObjectSelected) const;

	/** Create a widget that acts as a document for an animation asset */
	virtual TSharedRef<SWidget> CreateEditorWidgetForAnimDocument(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, UObject* InAnimAsset, const FAnimDocumentArgs& InArgs, FString& OutDocumentLink);

	/** Customize a skeletal mesh details panel */
	virtual void CustomizeMeshDetails(const TSharedRef<IDetailsView>& InDetailsView, const TSharedRef<IPersonaToolkit>& InPersonaToolkit);

	/** Gets the extensibility managers for outside entities to extend persona editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() {return ToolBarExtensibilityManager;}

	/** Import a new asset using the supplied skeleton */
	virtual void ImportNewAsset(USkeleton* InSkeleton, EFBXImportType DefaultImportType);

	/** Check all animations & skeletal meshes for curve usage */
	virtual void TestSkeletonCurveNamesForUse(const TSharedRef<IEditableSkeleton>& InEditableSkeleton) const;

	/** Apply Compression to list of animations */
	virtual void ApplyCompression(TArray<TWeakObjectPtr<class UAnimSequence>>& AnimSequences);

	/** Export to FBX files of the list of animations */
	virtual void ExportToFBX(TArray<TWeakObjectPtr<class UAnimSequence>>& AnimSequences, USkeletalMesh* SkeletalMesh);

	/** Add looping interpolation to the list of animations */
	virtual void AddLoopingInterpolation(TArray<TWeakObjectPtr<class UAnimSequence>>& AnimSequences);

	/** Customize the details of a slot node for the specified details view */
	virtual void CustomizeSlotNodeDetails(const TSharedRef<class IDetailsView>& InDetailsView, FOnInvokeTab InOnInvokeTab);

	/** Create a Persona editor mode manager. Should be destroyed using plain ol' delete. Note: Only FPersonaEditMode-derived modes should be used with this manager! */
	virtual class IPersonaEditorModeManager* CreatePersonaEditorModeManager();

	/** Delegate used to query whether recording is active */
	FIsRecordingActive& OnIsRecordingActive() { return IsRecordingActiveDelegate; }

	/** Delegate used to start recording animation */
	FRecord& OnRecord() { return RecordDelegate; }

	/** Delegate used to stop recording animation */
	FStopRecording& OnStopRecording() { return StopRecordingDelegate; }

	/** Delegate used to get the currently recording animation */
	FGetCurrentRecording& OnGetCurrentRecording() { return GetCurrentRecordingDelegate; }

	/** Delegate used to get the currently recording animation time */
	FGetCurrentRecordingTime& OnGetCurrentRecordingTime() { return GetCurrentRecordingTimeDelegate; }

private:
	/** When a new AnimBlueprint is created, this will handle post creation work such as adding non-event default nodes */
	void OnNewBlueprintCreated(UBlueprint* InBlueprint);

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	/** Delegate used to query whether recording is active */
	FIsRecordingActive IsRecordingActiveDelegate;

	/** Delegate used to start recording animation */
	FRecord RecordDelegate;

	/** Delegate used to stop recording animation */
	FStopRecording StopRecordingDelegate;

	/** Delegate used to get the currently recording animation */
	FGetCurrentRecording GetCurrentRecordingDelegate;

	/** Delegate used to get the currently recording animation time */
	FGetCurrentRecordingTime GetCurrentRecordingTimeDelegate;

	/** Delegate used to tick the skelmesh component recording */
	FTickRecording TickRecordingDelegate;
};

