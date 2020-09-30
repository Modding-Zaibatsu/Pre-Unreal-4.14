// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "PersonaMode.h"
#include "Persona.h"
#include "SSkeletonAnimNotifies.h"
#include "IDocumentation.h"
#include "SAnimBlueprintParentPlayerList.h"
#include "SSkeletonSlotNames.h"
#include "SSkeletonBlendProfiles.h"
#include "SDockTab.h"
#include "SAdvancedPreviewDetailsTab.h"
#include "ISkeletonEditorModule.h"
#include "SPersonaDetails.h"
#include "Editor/KismetWidgets/Public/SSingleObjectDetailsPanel.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PersonaUtils.h"

#define LOCTEXT_NAMESPACE "PersonaModes"

/////////////////////////////////////////////////////
// FPersonaTabs

// Tab constants
const FName FPersonaTabs::MorphTargetsID("MorphTargetsTab");
const FName FPersonaTabs::AnimCurveViewID("AnimCurveViewerTab");
const FName FPersonaTabs::SkeletonTreeViewID("SkeletonTreeView");		//@TODO: Name
// Skeleton Pose manager
const FName FPersonaTabs::RetargetManagerID("RetargetManager");
// Anim Blueprint params
// Explorer
// Class Defaults
const FName FPersonaTabs::AnimBlueprintPreviewEditorID("AnimBlueprintPreviewEditor");

const FName FPersonaTabs::AnimBlueprintParentPlayerEditorID("AnimBlueprintParentPlayerEditor");
// Anim Document
const FName FPersonaTabs::ScrubberID("ScrubberTab");

// Toolbar
const FName FPersonaTabs::PreviewViewportID("Viewport");		//@TODO: Name
const FName FPersonaTabs::AssetBrowserID("SequenceBrowser");	//@TODO: Name
const FName FPersonaTabs::MirrorSetupID("MirrorSetupTab");
const FName FPersonaTabs::AnimBlueprintDebugHistoryID("AnimBlueprintDebugHistoryTab");
const FName FPersonaTabs::AnimAssetPropertiesID("AnimAssetPropertiesTab");
const FName FPersonaTabs::MeshAssetPropertiesID("MeshAssetPropertiesTab");
const FName FPersonaTabs::PreviewManagerID("AnimPreviewSetup");		//@TODO: Name
const FName FPersonaTabs::SkeletonAnimNotifiesID("SkeletonAnimNotifies");
const FName FPersonaTabs::SkeletonSlotNamesID("SkeletonSlotNames");
const FName FPersonaTabs::SkeletonSlotGroupNamesID("SkeletonSlotGroupNames");
const FName FPersonaTabs::BlendProfileManagerID("BlendProfileManager");

const FName FPersonaTabs::AdvancedPreviewSceneSettingsID("AdvancedPreviewTab");

const FName FPersonaTabs::DetailsID("DetailsTab");

/////////////////////////////////////////////////////
// FPersonaMode

// Mode constants
const FName FPersonaModes::SkeletonDisplayMode( "SkeletonName" );
const FName FPersonaModes::MeshEditMode( "MeshName" );
const FName FPersonaModes::PhysicsEditMode( "PhysicsName" );
const FName FPersonaModes::AnimationEditMode( "AnimationName" );
const FName FPersonaModes::AnimBlueprintEditMode( "GraphName" );

/////////////////////////////////////////////////////
// FPersonaAppMode

FPersonaAppMode::FPersonaAppMode(TSharedPtr<class FPersona> InPersona, FName InModeName)
	: FApplicationMode(InModeName, FPersonaModes::GetLocalizedMode)
{
	MyPersona = InPersona;

	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonTreeSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FAnimationAssetBrowserSummoner(InPersona, InPersona->GetPersonaToolkit(), FOnOpenNewAsset::CreateSP(InPersona.Get(), &FPersona::HandleOpenNewAsset), FOnAnimationSequenceBrowserCreated::CreateSP(InPersona.Get(), &FPersona::HandleAnimationSequenceBrowserCreated), true)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FPreviewViewportSummoner(InPersona, InPersona->GetSkeletonTree(), InPersona->GetPreviewScene(), InPersona->OnPostUndo, InPersona, FOnViewportCreated::CreateSP(InPersona.Get(), &FPersona::HandleViewportCreated), InModeName != FPersonaModes::AnimationEditMode, true)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FMorphTargetTabSummoner(InPersona, InPersona->GetPreviewScene(), InPersona->OnPostUndo)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FAnimCurveViewerTabSummoner(InPersona, InPersona->GetSkeletonTree()->GetEditableSkeleton(), InPersona->GetPreviewScene(), InPersona->OnCurvesChanged, InPersona->OnPostUndo)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonAnimNotifiesSummoner(InPersona, InPersona->GetSkeletonTree()->GetEditableSkeleton(), InPersona->OnAnimNotifiesChanged, InPersona->OnPostUndo, FOnObjectsSelected::CreateSP(InPersona.Get(), &FPersona::HandleObjectsSelected))));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FRetargetManagerTabSummoner(InPersona, InPersona->GetSkeletonTree()->GetEditableSkeleton(), InPersona->GetPreviewScene(), InPersona->OnPostUndo)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonSlotNamesSummoner(InPersona, InPersona->GetSkeletonTree()->GetEditableSkeleton(), InPersona->OnPostUndo, FOnObjectSelected::CreateSP(InPersona.Get(), &FPersona::HandleObjectSelected))));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FSkeletonBlendProfilesSummoner(InPersona)));
	PersonaTabFactories.RegisterFactory(MakeShareable(new FAdvancedPreviewSceneTabSummoner(InPersona, InPersona->GetPreviewScene())));
}

void FPersonaAppMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FPersona> Persona = MyPersona.Pin();
	
	// Mode-specific setup
	Persona->PushTabFactories(PersonaTabFactories);

	Persona->RegisterToolbarTab(InTabManager.ToSharedRef());
}

void FPersonaAppMode::PostActivateMode()
{
	FApplicationMode::PostActivateMode();

	if(MyPersona.IsValid())
	{
		MyPersona.Pin()->ReinitMode();
	}
}
/////////////////////////////////////////////////////
// FPersonaModeSharedData

FPersonaModeSharedData::FPersonaModeSharedData()
	: OrthoZoom(1.0f)
	, bCameraLock(true)
	, bCameraFollow(false)
	, bShowReferencePose(false)
	, bShowBones(false)
	, bShowBoneNames(false)
	, bShowSockets(false)
	, bShowBound(false)
	, ViewportType(0)
	, PlaybackSpeedMode(EAnimationPlaybackSpeeds::Normal)
	, LocalAxesMode(0)
{}

void FPersonaModeSharedData::Save(const TSharedRef<FAnimationViewportClient>& InFromViewport)
{
	ViewLocation = InFromViewport->GetViewLocation();
	ViewRotation = InFromViewport->GetViewRotation();
	LookAtLocation = InFromViewport->GetLookAtLocation();
	OrthoZoom = InFromViewport->GetOrthoZoom();
	bCameraLock = InFromViewport->IsCameraLocked();
	bCameraFollow = InFromViewport->IsSetCameraFollowChecked();
	bShowBound = InFromViewport->IsSetShowBoundsChecked();
	LocalAxesMode = InFromViewport->GetLocalAxesMode();
	ViewportType = InFromViewport->ViewportType;
	PlaybackSpeedMode = InFromViewport->GetPlaybackSpeedMode();
}

void FPersonaModeSharedData::Restore(const TSharedRef<FAnimationViewportClient>& InToViewport)
{	
	InToViewport->SetViewportType((ELevelViewportType)ViewportType);
	InToViewport->SetViewLocation(ViewLocation);
	InToViewport->SetViewRotation(ViewRotation);
	InToViewport->SetShowBounds(bShowBound);
	InToViewport->SetLocalAxesMode((ELocalAxesMode::Type)LocalAxesMode);
	InToViewport->SetOrthoZoom(OrthoZoom);
	InToViewport->SetPlaybackSpeedMode(PlaybackSpeedMode);
	
	if (bCameraLock)
	{
		InToViewport->SetLookAtLocation(LookAtLocation);
	}
	else if(bCameraFollow)
	{
		InToViewport->SetCameraFollow();
	}
}

/////////////////////////////////////////////////////
// FSkeletonTreeSummoner

FSkeletonTreeSummoner::FSkeletonTreeSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FPersonaTabs::SkeletonTreeViewID, InHostingApp)
{
	TabLabel = LOCTEXT("SkeletonTreeTabTitle", "Skeleton Tree");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.Tabs.SkeletonTree");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("SkeletonTreeView", "Skeleton Tree");
	ViewMenuTooltip = LOCTEXT("SkeletonTreeView_ToolTip", "Shows the skeleton tree");
}

TSharedRef<SWidget> FSkeletonTreeSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FPersona> Persona = StaticCastSharedPtr<FPersona>(HostingApp.Pin());
	return Persona->GetSkeletonTree();
}

/////////////////////////////////////////////////////
// FMorphTargetTabSummoner

#include "SMorphTargetViewer.h"

FMorphTargetTabSummoner::FMorphTargetTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo)
	: FWorkflowTabFactory(FPersonaTabs::MorphTargetsID, InHostingApp)
	, PreviewScene(InPreviewScene)
	, OnPostUndo(InOnPostUndo)
{
	TabLabel = LOCTEXT("MorphTargetTabTitle", "Morph Target Previewer");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.Tabs.MorphTargetPreviewer");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("MorphTargetTabView", "Morph Target Previewer");
	ViewMenuTooltip = LOCTEXT("MorphTargetTabView_ToolTip", "Shows the morph target viewer");
}

TSharedRef<SWidget> FMorphTargetTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SMorphTargetViewer, PreviewScene.Pin().ToSharedRef(), OnPostUndo);
}
/////////////////////////////////////////////////////
// FAnimCurveViewerTabSummoner

#include "SAnimCurveViewer.h"

FAnimCurveViewerTabSummoner::FAnimCurveViewerTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnCurvesChanged, FSimpleMulticastDelegate& InOnPostUndo)
	: FWorkflowTabFactory(FPersonaTabs::AnimCurveViewID, InHostingApp)
	, EditableSkeleton(InEditableSkeleton)
	, PreviewScene(InPreviewScene)
	, OnCurvesChanged(InOnCurvesChanged)
	, OnPostUndo(InOnPostUndo)
{
	TabLabel = LOCTEXT("AnimCurveViewTabTitle", "Anim Curves");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.Tabs.AnimCurvePreviewer");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("AnimCurveTabView", "Animation Curves");
	ViewMenuTooltip = LOCTEXT("AnimCurveTabView_ToolTip", "Shows the animation curve viewer");
}

TSharedRef<SWidget> FAnimCurveViewerTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SAnimCurveViewer, EditableSkeleton.Pin().ToSharedRef(), PreviewScene.Pin().ToSharedRef(), OnCurvesChanged, OnPostUndo);
}

/////////////////////////////////////////////////////
// FAnimationAssetBrowserSummoner

#include "SAnimationSequenceBrowser.h"

FAnimationAssetBrowserSummoner::FAnimationAssetBrowserSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<IPersonaToolkit>& InPersonaToolkit, FOnOpenNewAsset InOnOpenNewAsset, FOnAnimationSequenceBrowserCreated InOnAnimationSequenceBrowserCreated, bool bInShowHistory)
	: FWorkflowTabFactory(FPersonaTabs::AssetBrowserID, InHostingApp)
	, PersonaToolkit(InPersonaToolkit)
	, OnOpenNewAsset(InOnOpenNewAsset)
	, OnAnimationSequenceBrowserCreated(InOnAnimationSequenceBrowserCreated)
	, bShowHistory(bInShowHistory)
{
	TabLabel = LOCTEXT("AssetBrowserTabTitle", "Asset Browser");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("AssetBrowser", "Asset Browser");
	ViewMenuTooltip = LOCTEXT("AssetBrowser_ToolTip", "Shows the animation asset browser");
}

TSharedRef<SWidget> FAnimationAssetBrowserSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedRef<SAnimationSequenceBrowser> Widget = SNew(SAnimationSequenceBrowser, PersonaToolkit.Pin().ToSharedRef())
		.OnOpenNewAsset(OnOpenNewAsset)
		.ShowHistory(bShowHistory);

	OnAnimationSequenceBrowserCreated.ExecuteIfBound(Widget);

	return Widget;
}

/////////////////////////////////////////////////////
// FPreviewViewportSummoner

#include "SAnimationEditorViewport.h"

FPreviewViewportSummoner::FPreviewViewportSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<ISkeletonTree>& InSkeletonTree, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo, const TSharedPtr<FBlueprintEditor>& InBlueprintEditor, FOnViewportCreated InOnViewportCreated, bool bInShowTimeline, bool bInShowStats)
	: FWorkflowTabFactory(FPersonaTabs::PreviewViewportID, InHostingApp)
	, SkeletonTree(InSkeletonTree)
	, PreviewScene(InPreviewScene)
	, OnPostUndo(InOnPostUndo)
	, BlueprintEditor(InBlueprintEditor)
	, OnViewportCreated(InOnViewportCreated)
	, bShowTimeline(bInShowTimeline)
	, bShowStats(bInShowStats)
{
	TabLabel = LOCTEXT("ViewportTabTitle", "Viewport");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("ViewportView", "Viewport");
	ViewMenuTooltip = LOCTEXT("ViewportView_ToolTip", "Shows the viewport");
}

TSharedRef<SWidget> FPreviewViewportSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedRef<SAnimationEditorViewportTabBody> NewViewport = SNew(SAnimationEditorViewportTabBody, SkeletonTree.Pin().ToSharedRef(), PreviewScene.Pin().ToSharedRef(), HostingApp.Pin().ToSharedRef(), OnPostUndo)
		.BlueprintEditor(BlueprintEditor.Pin())
		.OnInvokeTab(FOnInvokeTab::CreateSP(HostingApp.Pin().Get(), &FAssetEditorToolkit::InvokeTab))
		.AddMetaData<FTagMetaData>(TEXT("Persona.Viewport"))
		.ShowTimeline(bShowTimeline)
		.ShowStats(bShowStats);

	OnViewportCreated.ExecuteIfBound(NewViewport);

	return NewViewport;
}

/////////////////////////////////////////////////////
// FRetargetManagerTabSummoner

#include "SRetargetManager.h"

FRetargetManagerTabSummoner::FRetargetManagerTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo)
	: FWorkflowTabFactory(FPersonaTabs::RetargetManagerID, InHostingApp)
	, EditableSkeleton(InEditableSkeleton)
	, PreviewScene(InPreviewScene)
	, OnPostUndo(InOnPostUndo)
{
	TabLabel = LOCTEXT("RetargetManagerTabTitle", "Retarget Manager");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.Tabs.RetargetManager");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("RetargetManagerTabView", "Retarget Manager");
	ViewMenuTooltip = LOCTEXT("RetargetManagerTabView_ToolTip", "Manages different options for retargeting");
}

TSharedRef<SWidget> FRetargetManagerTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SRetargetManager, EditableSkeleton.Pin().ToSharedRef(), PreviewScene.Pin().ToSharedRef(), OnPostUndo);
}


/////////////////////////////////////////////////////
// SPersonaPreviewPropertyEditor

void SPersonaPreviewPropertyEditor::Construct(const FArguments& InArgs, TSharedRef<IPersonaPreviewScene> InPreviewScene)
{
	PreviewScene = InPreviewScene;
	bPropertyEdited = false;

	SSingleObjectDetailsPanel::Construct(SSingleObjectDetailsPanel::FArguments(), /*bAutomaticallyObserveViaGetObjectToObserve*/ true, /*bAllowSearch*/ true);

	PropertyView->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic([] { return !GIntraFrameDebuggingGameThread; }));
	PropertyView->OnFinishedChangingProperties().Add(FOnFinishedChangingProperties::FDelegate::CreateSP(this, &SPersonaPreviewPropertyEditor::HandlePropertyChanged));
}

UObject* SPersonaPreviewPropertyEditor::GetObjectToObserve() const
{
	if (UDebugSkelMeshComponent* PreviewMeshComponent = PreviewScene.Pin()->GetPreviewMeshComponent())
	{
		if (PreviewMeshComponent->GetAnimInstance() != nullptr)
		{
			return PreviewMeshComponent->GetAnimInstance();
		}
	}

	return nullptr;
}

TSharedRef<SWidget> SPersonaPreviewPropertyEditor::PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget)
{
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			PropertyEditorWidget
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Docking.Tab.ContentAreaBrush"))
			.Visibility_Lambda([this]() { return bPropertyEdited ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AnimBlueprintEditPreviewText", "Changes made to preview only. Changes will not be saved!"))
					.ColorAndOpacity(FLinearColor::Yellow)
					.ShadowOffset(FVector2D::UnitVector)
					.AutoWrapText(true)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				[
					SNew(SButton)
					.OnClicked(this, &SPersonaPreviewPropertyEditor::HandleApplyChanges)
					.ToolTipText(LOCTEXT("AnimBlueprintEditApplyChanges_Tooltip", "Apply any changes that have been made to the preview to the defaults."))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AnimBlueprintEditApplyChanges", "Apply"))
					]
				]
			]
		];
}

void SPersonaPreviewPropertyEditor::HandlePropertyChanged(const FPropertyChangedEvent& InPropertyChangedEvent)
{
	if (UDebugSkelMeshComponent* PreviewMeshComponent = PreviewScene.Pin()->GetPreviewMeshComponent())
	{
		if (UAnimInstance* AnimInstance = PreviewMeshComponent->GetAnimInstance())
		{
			// check to see how many properties have changed
			const int32 NumChangedProperties = PersonaUtils::CopyPropertiesToCDO(AnimInstance, PersonaUtils::FCopyOptions(PersonaUtils::ECopyOptions::PreviewOnly));
			bPropertyEdited = (NumChangedProperties > 0);
		}
	}
}

FReply SPersonaPreviewPropertyEditor::HandleApplyChanges()
{
	// copy preview properties into CDO
	if (UDebugSkelMeshComponent* PreviewMeshComponent = PreviewScene.Pin()->GetPreviewMeshComponent())
	{
		if (UAnimInstance* AnimInstance = PreviewMeshComponent->GetAnimInstance())
		{
			PersonaUtils::CopyPropertiesToCDO(AnimInstance);

			bPropertyEdited = false;
		}
	}

	return FReply::Handled();
}

/////////////////////////////////////////////////////
// FAnimBlueprintPreviewEditorSummoner

#include "SKismetInspector.h"

FAnimBlueprintPreviewEditorSummoner::FAnimBlueprintPreviewEditorSummoner(TSharedPtr<class FBlueprintEditor> InBlueprintEditor, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene)
	: FWorkflowTabFactory(FPersonaTabs::AnimBlueprintPreviewEditorID, InBlueprintEditor)
	, BlueprintEditor(InBlueprintEditor)
	, PreviewScene(InPreviewScene)
{
	TabLabel = LOCTEXT("AnimBlueprintPreviewTabTitle", "Anim Preview Editor");

	bIsSingleton = true;

	CurrentMode = EAnimBlueprintEditorMode::PreviewMode;

	ViewMenuDescription = LOCTEXT("AnimBlueprintPreviewView", "Preview");
	ViewMenuTooltip = LOCTEXT("AnimBlueprintPreviewView_ToolTip", "Shows the animation preview editor view (as well as class defaults)");
}

TSharedRef<SWidget> FAnimBlueprintPreviewEditorSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return	SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(FMargin( 0.f, 0.f, 2.f, 0.f ))
				[
					SNew(SBorder)
					.BorderImage(this, &FAnimBlueprintPreviewEditorSummoner::GetBorderBrushByMode, EAnimBlueprintEditorMode::PreviewMode)
					.Padding(0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked(this, &FAnimBlueprintPreviewEditorSummoner::IsChecked, EAnimBlueprintEditorMode::PreviewMode)
						.OnCheckStateChanged(this, &FAnimBlueprintPreviewEditorSummoner::OnCheckedChanged, EAnimBlueprintEditorMode::PreviewMode)
						.ToolTip(IDocumentation::Get()->CreateToolTip(	LOCTEXT("AnimBlueprintPropertyEditorPreviewMode", "Switch to editing the preview instance properties"),
																		NULL,
																		TEXT("Shared/Editors/Persona"),
																		TEXT("AnimBlueprintPropertyEditorPreviewMode")))
						[
							SNew( STextBlock )
							.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 9 ) )
							.Text( LOCTEXT("AnimBlueprintDefaultsPreviewMode", "Edit Preview") )
						]
					]
				]
				+SHorizontalBox::Slot()
				.Padding(FMargin( 2.f, 0.f, 0.f, 0.f ))
				[
					SNew(SBorder)
					.BorderImage(this, &FAnimBlueprintPreviewEditorSummoner::GetBorderBrushByMode, EAnimBlueprintEditorMode::DefaultsMode)
					.Padding(0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked(this, &FAnimBlueprintPreviewEditorSummoner::IsChecked, EAnimBlueprintEditorMode::DefaultsMode)
						.OnCheckStateChanged(this, &FAnimBlueprintPreviewEditorSummoner::OnCheckedChanged, EAnimBlueprintEditorMode::DefaultsMode)
						.ToolTip(IDocumentation::Get()->CreateToolTip(	LOCTEXT("AnimBlueprintPropertyEditorDefaultMode", "Switch to editing the class defaults"),
																		NULL,
																		TEXT("Shared/Editors/Persona"),
																		TEXT("AnimBlueprintPropertyEditorDefaultMode")))
						[
							SNew( STextBlock )
							.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 9 ) )
							.Text( LOCTEXT("AnimBlueprintDefaultsDefaultsMode", "Edit Defaults") )
						]
					]
				]
			]
			+SVerticalBox::Slot()
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SBorder)
					.Padding(0)
					.BorderImage( FEditorStyle::GetBrush("NoBorder") )
					.Visibility(this, &FAnimBlueprintPreviewEditorSummoner::IsEditorVisible, EAnimBlueprintEditorMode::PreviewMode)
					[
						SNew(SPersonaPreviewPropertyEditor, PreviewScene.Pin().ToSharedRef())
					]
				]
				+SOverlay::Slot()
				[
					SNew(SBorder)
					.Padding(FMargin(3.0f, 2.0f))
					.BorderImage( FEditorStyle::GetBrush("NoBorder") )
					.Visibility(this, &FAnimBlueprintPreviewEditorSummoner::IsEditorVisible, EAnimBlueprintEditorMode::DefaultsMode)
					[
						BlueprintEditor.Pin()->GetDefaultEditor()
					]
				]
			];
}

FText FAnimBlueprintPreviewEditorSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("AnimBlueprintPreviewEditorTooltip", "The editor lets you change the values of the preview instance");
}

EVisibility FAnimBlueprintPreviewEditorSummoner::IsEditorVisible(EAnimBlueprintEditorMode::Type Mode) const
{
	return CurrentMode == Mode ? EVisibility::Visible: EVisibility::Hidden;
}

ECheckBoxState FAnimBlueprintPreviewEditorSummoner::IsChecked(EAnimBlueprintEditorMode::Type Mode) const
{
	return CurrentMode == Mode ? ECheckBoxState::Checked: ECheckBoxState::Unchecked;
}

const FSlateBrush* FAnimBlueprintPreviewEditorSummoner::GetBorderBrushByMode(EAnimBlueprintEditorMode::Type Mode) const
{
	if(Mode == CurrentMode)
	{
		return FEditorStyle::GetBrush("ModeSelector.ToggleButton.Pressed");
	}
	else
	{
		return FEditorStyle::GetBrush("ModeSelector.ToggleButton.Normal");
	}
}

void FAnimBlueprintPreviewEditorSummoner::OnCheckedChanged(ECheckBoxState NewType, EAnimBlueprintEditorMode::Type Mode)
{
	if(NewType == ECheckBoxState::Checked)
	{
		CurrentMode = Mode;
	}
}

//////////////////////////////////////////////////////////////////////////
// FAnimBlueprintParentPlayerEditorSummoner

FAnimBlueprintParentPlayerEditorSummoner::FAnimBlueprintParentPlayerEditorSummoner(TSharedPtr<class FBlueprintEditor> InBlueprintEditor, FSimpleMulticastDelegate& InOnPostUndo)
	: FWorkflowTabFactory(FPersonaTabs::AnimBlueprintParentPlayerEditorID, InBlueprintEditor)
	, BlueprintEditor(InBlueprintEditor)
	, OnPostUndo(InOnPostUndo)
{
	TabLabel = LOCTEXT("ParentPlayerOverrideEditor", "Asset Override Editor");
	bIsSingleton = true;
}

TSharedRef<SWidget> FAnimBlueprintParentPlayerEditorSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SAnimBlueprintParentPlayerList, BlueprintEditor.Pin().ToSharedRef(), OnPostUndo);
}

FText FAnimBlueprintParentPlayerEditorSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("AnimSubClassTabToolTip", "Editor for overriding the animation assets referenced by the parent animation graph.");
}

/////////////////////////////////////////////////////
// FAdvancedPreviewSceneTabSummoner

FAdvancedPreviewSceneTabSummoner::FAdvancedPreviewSceneTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TSharedRef<IPersonaPreviewScene>& InPreviewScene)
	: FWorkflowTabFactory(FPersonaTabs::AdvancedPreviewSceneSettingsID, InHostingApp)
	, PreviewScene(InPreviewScene)
{
	TabLabel = LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details");	
	bIsSingleton = true;
	
	ViewMenuDescription = LOCTEXT("AdvancedPreviewScene", "Preview Scene Settings");
	ViewMenuTooltip = LOCTEXT("AdvancedPreviewScene_ToolTip", "Shows the advanced preview scene settings");
}

TSharedRef<SWidget> FAdvancedPreviewSceneTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SAdvancedPreviewDetailsTab, PreviewScene.Pin().ToSharedRef());
}

FText FAdvancedPreviewSceneTabSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("AdvancedPreviewSettingsToolTip", "The Advanced Preview Settings tab will let you alter the preview scene's settings.");
}

/////////////////////////////////////////////////////
// FDetailsTabSummoner

FDetailsTabSummoner::FDetailsTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, FOnDetailsCreated InOnDetailsCreated)
	: FWorkflowTabFactory(FPersonaTabs::DetailsID, InHostingApp)
	, OnDetailsCreated(InOnDetailsCreated)
{
	TabLabel = LOCTEXT("PersonaDetailsTab", "Details");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details");
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("DetailsDescription", "Details");
	ViewMenuTooltip = LOCTEXT("DetailsToolTip", "Shows the details tab for selected objects.");

	PersonaDetails = SNew(SPersonaDetails);

	OnDetailsCreated.ExecuteIfBound(PersonaDetails->DetailsView.ToSharedRef());
}

TSharedRef<SWidget> FDetailsTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return PersonaDetails.ToSharedRef();
}

FText FDetailsTabSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return LOCTEXT("PersonaDetailsToolTip", "Edit the details of selected objects.");
}


#undef LOCTEXT_NAMESPACE

