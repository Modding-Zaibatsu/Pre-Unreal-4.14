// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SkeletonEditorPrivatePCH.h"
#include "ISkeletonEditorModule.h"
#include "SkeletonEditor.h"
#include "IPersonaToolkit.h"
#include "PersonaModule.h"
#include "SkeletonEditorMode.h"
#include "IPersonaPreviewScene.h"
#include "SkeletonEditorCommands.h"
#include "Factories/FbxImportUI.h"
#include "IEditableSkeleton.h"
#include "IDetailsView.h"
#include "ISkeletonTree.h"
#include "IAssetFamily.h"
#include "AssetEditorModeManager.h"

const FName SkeletonEditorAppIdentifier = FName(TEXT("SkeletonEditorApp"));

const FName SkeletonEditorModes::SkeletonEditorMode(TEXT("SkeletonEditorMode"));

const FName SkeletonEditorTabs::DetailsTab(TEXT("DetailsTab"));
const FName SkeletonEditorTabs::SkeletonTreeTab(TEXT("SkeletonTreeView"));
const FName SkeletonEditorTabs::ViewportTab(TEXT("Viewport"));
const FName SkeletonEditorTabs::AnimNotifiesTab(TEXT("SkeletonAnimNotifies"));
const FName SkeletonEditorTabs::CurveNamesTab(TEXT("AnimCurveViewerTab"));
const FName SkeletonEditorTabs::AdvancedPreviewTab(TEXT("AdvancedPreviewTab"));
const FName SkeletonEditorTabs::RetargetManagerTab(TEXT("RetargetManager"));
const FName SkeletonEditorTabs::SlotNamesTab("SkeletonSlotNames");

DEFINE_LOG_CATEGORY(LogSkeletonEditor);

#define LOCTEXT_NAMESPACE "SkeletonEditor"

FSkeletonEditor::FSkeletonEditor()
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}
}

FSkeletonEditor::~FSkeletonEditor()
{
	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->UnregisterForUndo(this);
	}
}

void FSkeletonEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_SkeletonEditor", "Skeleton Editor"));

	FAssetEditorToolkit::RegisterTabSpawners( InTabManager );
}

void FSkeletonEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FSkeletonEditor::InitSkeletonEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, USkeleton* InSkeleton)
{
	Skeleton = InSkeleton;

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(InSkeleton);

	PersonaToolkit->GetPreviewScene()->SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode::ReferencePose);

	TSharedRef<IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(InSkeleton);
	AssetFamily->RecordAssetOpened(FAssetData(InSkeleton));

	FSkeletonTreeArgs SkeletonTreeArgs(OnPostUndo);
	SkeletonTreeArgs.OnObjectSelected = FOnObjectSelected::CreateSP(this, &FSkeletonEditor::HandleObjectSelected);
	SkeletonTreeArgs.PreviewScene = PersonaToolkit->GetPreviewScene();

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::GetModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	SkeletonTree = SkeletonEditorModule.CreateSkeletonTree(PersonaToolkit->GetSkeleton(), PersonaToolkit->GetMesh(), SkeletonTreeArgs);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, SkeletonEditorAppIdentifier, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InSkeleton);

	BindCommands();

	AddApplicationMode(
		SkeletonEditorModes::SkeletonEditorMode,
		MakeShareable(new FSkeletonEditorMode(SharedThis(this), SkeletonTree.ToSharedRef())));

	SetCurrentMode(SkeletonEditorModes::SkeletonEditorMode);

	// set up our editor mode
	check(AssetEditorModeManager);
	AssetEditorModeManager->SetDefaultMode(FPersonaEditModes::SkeletonSelection);

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

FName FSkeletonEditor::GetToolkitFName() const
{
	return FName("SkeletonEditor");
}

FText FSkeletonEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "SkeletonEditor");
}

FString FSkeletonEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "SkeletonEditor ").ToString();
}

FLinearColor FSkeletonEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void FSkeletonEditor::BindCommands()
{
	FSkeletonEditorCommands::Register();
	
	ToolkitCommands->MapAction( FSkeletonEditorCommands::Get().RemoveUnusedBones,
		FExecuteAction::CreateSP(this, &FSkeletonEditor::RemoveUnusedBones),
		FCanExecuteAction::CreateSP(this, &FSkeletonEditor::CanRemoveBones));

	ToolkitCommands->MapAction(FSkeletonEditorCommands::Get().TestSkeletonCurveNamesForUse,
		FExecuteAction::CreateSP(this, &FSkeletonEditor::TestSkeletonCurveNamesForUse));

	ToolkitCommands->MapAction(FSkeletonEditorCommands::Get().UpdateSkeletonRefPose,
		FExecuteAction::CreateSP(this, &FSkeletonEditor::UpdateSkeletonRefPose));

	ToolkitCommands->MapAction(FSkeletonEditorCommands::Get().AnimNotifyWindow,
		FExecuteAction::CreateSP(this, &FSkeletonEditor::OnAnimNotifyWindow));

	ToolkitCommands->MapAction(FSkeletonEditorCommands::Get().RetargetManager,
		FExecuteAction::CreateSP(this, &FSkeletonEditor::OnRetargetManager));

	ToolkitCommands->MapAction(FSkeletonEditorCommands::Get().ImportMesh,
		FExecuteAction::CreateSP(this, &FSkeletonEditor::OnImportAsset));
}

void FSkeletonEditor::ExtendToolbar()
{
	// If the ToolbarExtender is valid, remove it before rebuilding it
	if (ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::GetModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	AddToolbarExtender(SkeletonEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	TArray<ISkeletonEditorModule::FSkeletonEditorToolbarExtender> ToolbarExtenderDelegates = SkeletonEditorModule.GetAllSkeletonEditorToolbarExtenders();

	for (auto& ToolbarExtenderDelegate : ToolbarExtenderDelegates)
	{
		if (ToolbarExtenderDelegate.IsBound())
		{
			AddToolbarExtender(ToolbarExtenderDelegate.Execute(GetToolkitCommands(), SharedThis(this)));
		}
	}

	// extend extra menu/toolbars
	struct Local
	{
		static void ExtendToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Skeleton");
			{
				ToolbarBuilder.AddToolBarButton(FSkeletonEditorCommands::Get().AnimNotifyWindow);
				ToolbarBuilder.AddToolBarButton(FSkeletonEditorCommands::Get().RetargetManager, NAME_None, LOCTEXT("Toolbar_RetargetManager", "Retarget Manager"));
				ToolbarBuilder.AddToolBarButton(FSkeletonEditorCommands::Get().ImportMesh);
			}
			ToolbarBuilder.EndSection();
		}
	};

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::ExtendToolbar)
		);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ParentToolbarBuilder)
		{
			FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
			TSharedRef<class IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(Skeleton);
			AddToolbarWidget(PersonaModule.CreateAssetFamilyShortcutWidget(SharedThis(this), AssetFamily));
		}	
	));
}

void FSkeletonEditor::ExtendMenu()
{
	MenuExtender = MakeShareable(new FExtender);

	struct Local
	{
		static void ExtendMenu(FMenuBuilder& MenuBuilder)
		{
			// View
			MenuBuilder.BeginSection("SkeletonEditor", LOCTEXT("SkeletonEditorAssetMenu_Skeleton", "Skeleton"));
			{
				MenuBuilder.AddMenuEntry(FSkeletonEditorCommands::Get().RemoveUnusedBones);
				MenuBuilder.AddMenuEntry(FSkeletonEditorCommands::Get().UpdateSkeletonRefPose);
				MenuBuilder.AddMenuEntry(FSkeletonEditorCommands::Get().TestSkeletonCurveNamesForUse);
			}
			MenuBuilder.EndSection();
		}
	};

	MenuExtender->AddMenuExtension(
		"AssetEditorActions",
		EExtensionHook::After,
		GetToolkitCommands(),
		FMenuExtensionDelegate::CreateStatic(&Local::ExtendMenu)
		);

	AddMenuExtender(MenuExtender);

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::GetModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	AddMenuExtender(SkeletonEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FSkeletonEditor::HandleObjectsSelected(const TArray<UObject*>& InObjects)
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObjects(InObjects);
	}
}

void FSkeletonEditor::HandleObjectSelected(UObject* InObject)
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(InObject);
	}
}

void FSkeletonEditor::PostUndo(bool bSuccess)
{
	OnPostUndo.Broadcast();
}

void FSkeletonEditor::PostRedo(bool bSuccess)
{
	OnPostUndo.Broadcast();
}

bool FSkeletonEditor::CanRemoveBones() const
{
	UDebugSkelMeshComponent* PreviewMeshComponent = PersonaToolkit->GetPreviewMeshComponent();
	return PreviewMeshComponent && PreviewMeshComponent->SkeletalMesh;
}

void FSkeletonEditor::RemoveUnusedBones()
{
	GetSkeletonTree()->GetEditableSkeleton()->RemoveUnusedBones();
}

void FSkeletonEditor::TestSkeletonCurveNamesForUse() const
{
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	PersonaModule.TestSkeletonCurveNamesForUse(GetSkeletonTree()->GetEditableSkeleton());
}

void FSkeletonEditor::UpdateSkeletonRefPose()
{
	GetSkeletonTree()->GetEditableSkeleton()->UpdateSkeletonReferencePose(PersonaToolkit->GetPreviewMeshComponent()->SkeletalMesh);
}

void FSkeletonEditor::OnAnimNotifyWindow()
{
	InvokeTab(SkeletonEditorTabs::AnimNotifiesTab);
}

void FSkeletonEditor::OnRetargetManager()
{
	InvokeTab(SkeletonEditorTabs::RetargetManagerTab);
}

void FSkeletonEditor::OnImportAsset()
{
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	PersonaModule.ImportNewAsset(Skeleton, FBXIT_SkeletalMesh);
}

void FSkeletonEditor::HandleDetailsCreated(const TSharedRef<IDetailsView>& InDetailsView)
{
	DetailsView = InDetailsView;
}

#undef LOCTEXT_NAMESPACE
