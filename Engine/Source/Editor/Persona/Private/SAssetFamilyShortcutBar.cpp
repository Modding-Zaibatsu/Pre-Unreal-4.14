// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "SAssetFamilyShortcutBar.h"
#include "IAssetFamily.h"
#include "AssetThumbnail.h"
#include "SHyperlink.h"
#include "BreakIterator.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "WorkflowCentricApplication.h"

#define LOCTEXT_NAMESPACE "SAssetFamilyShortcutBar"

namespace AssetShortcutConstants
{
	const int32 ThumbnailSize = 40;
	const int32 ThumbnailSizeSmall = 16;
}

class SAssetShortcut : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetShortcut)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IAssetFamily>& InAssetFamily, const FAssetData& InAssetData, const TSharedRef<FAssetThumbnailPool>& InThumbnailPool)
	{
		AssetData = InAssetData;
		AssetFamily = InAssetFamily;
		HostingApp = InHostingApp;
		ThumbnailPoolPtr = InThumbnailPool;
		bPackageDirty = false;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnFilesLoaded().AddSP(this, &SAssetShortcut::HandleFilesLoaded);
		AssetRegistryModule.Get().OnAssetAdded().AddSP(this, &SAssetShortcut::HandleAssetAdded);
		AssetRegistryModule.Get().OnAssetRemoved().AddSP(this, &SAssetShortcut::HandleAssetRemoved);

		FAssetEditorManager::Get().OnAssetEditorRequestedOpen().AddSP(this, &SAssetShortcut::HandleAssetOpened);
		AssetFamily->GetOnAssetOpened().AddSP(this, &SAssetShortcut::HandleAssetOpened);

		AssetThumbnail = MakeShareable(new FAssetThumbnail(InAssetData, AssetShortcutConstants::ThumbnailSize, AssetShortcutConstants::ThumbnailSize, InThumbnailPool));
		AssetThumbnailSmall = MakeShareable(new FAssetThumbnail(InAssetData, AssetShortcutConstants::ThumbnailSizeSmall, AssetShortcutConstants::ThumbnailSizeSmall, InThumbnailPool));

		TArray<FAssetData> Assets;
		InAssetFamily->FindAssetsOfType(InAssetData.GetClass(), Assets);
		bMultipleAssetsExist = Assets.Num() > 1;
		AssetDirtyBrush = FEditorStyle::GetBrush("ContentBrowser.ContentDirty");

		ChildSlot
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SAssignNew(CheckBox, SCheckBox)
				.Style(FEditorStyle::Get(), "ToolBar.ToggleButton")
				.ForegroundColor(FSlateColor::UseForeground())
				.Padding(0.0f)
				.OnCheckStateChanged(this, &SAssetShortcut::HandleOpenAssetShortcut)
				.IsChecked(this, &SAssetShortcut::GetCheckState)
				.Visibility(this, &SAssetShortcut::GetButtonVisibility)
				.ToolTipText(FText::FromString(InAssetData.GetExportTextName()))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SBorder)
						.Padding(4.0f)
						.BorderImage(FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow"))
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							[
								SAssignNew(ThumbnailBox, SBox)
								.WidthOverride(AssetShortcutConstants::ThumbnailSize)
								.HeightOverride(AssetShortcutConstants::ThumbnailSize)
								.Visibility(this, &SAssetShortcut::GetThumbnailVisibility)
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										AssetThumbnail->MakeThumbnailWidget()
									]
									+SOverlay::Slot()
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Bottom)
									[
										SNew(SImage)
										.Image(this, &SAssetShortcut::GetDirtyImage)
									]
								]
							]
							+SHorizontalBox::Slot()
							[
								SAssignNew(ThumbnailSmallBox, SBox)
								.WidthOverride(AssetShortcutConstants::ThumbnailSizeSmall)
								.HeightOverride(AssetShortcutConstants::ThumbnailSizeSmall)
								.Visibility(this, &SAssetShortcut::GetSmallThumbnailVisibility)
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										AssetThumbnailSmall->MakeThumbnailWidget()
									]
									+SOverlay::Slot()
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Bottom)
									[
										SNew(SImage)
										.Image(this, &SAssetShortcut::GetDirtyImage)
									]
								]
							]
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(AssetShortcutConstants::ThumbnailSize + 16.0f)
						.HeightOverride(AssetShortcutConstants::ThumbnailSize + 4.0f)
						.VAlign(VAlign_Center)
						.Visibility(this, &SAssetShortcut::GetThumbnailVisibility)
						[
							SNew(STextBlock)
							.Font(FEditorStyle::GetFontStyle("ContentBrowser.AssetTileViewNameFontSmall"))
							.Text(this, &SAssetShortcut::GetAssetText)
							.ColorAndOpacity(this, &SAssetShortcut::GetAssetTextColor)
							.Justification(ETextJustify::Center)
							.LineBreakPolicy(FBreakIterator::CreateCamelCaseBreakIterator())
							.AutoWrapText(true)
						]
					]
				]
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.AutoWidth()
			[
				SNew(SComboButton)
				.Visibility(this, &SAssetShortcut::GetComboVisibility)
				.ContentPadding(0)
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FEditorStyle::Get(), "Toolbar.Button")
				.OnGetMenuContent(this, &SAssetShortcut::HandleGetMenuContent)
			]
		];

		EnableToolTipForceField(true);

		DirtyStateTimerHandle = RegisterActiveTimer(1.0f / 10.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SAssetShortcut::HandleRefreshDirtyState));
	}

	~SAssetShortcut()
	{
		if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			AssetRegistryModule.Get().OnFilesLoaded().RemoveAll(this);
			AssetRegistryModule.Get().OnAssetAdded().RemoveAll(this);
			AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);
		}

		AssetFamily->GetOnAssetOpened().RemoveAll(this);
		FAssetEditorManager::Get().OnAssetEditorRequestedOpen().RemoveAll(this);
		UnRegisterActiveTimer(DirtyStateTimerHandle.ToSharedRef());
	}

	void HandleOpenAssetShortcut(ECheckBoxState InState)
	{
		TArray<UObject*> Assets;
		Assets.Add(AssetData.GetAsset());
		FAssetEditorManager::Get().OpenEditorForAssets(Assets);
	}

	FText GetAssetText() const
	{
		return FText::FromName(AssetData.AssetName);
	}

	ECheckBoxState GetCheckState() const
	{
		const TArray<UObject*>* Objects = HostingApp.Pin()->GetObjectsCurrentlyBeingEdited();
		if (Objects != nullptr)
		{
			for (UObject* Object : *Objects)
			{
				if (FAssetData(Object) == AssetData)
				{
					return ECheckBoxState::Checked;
				}
			}
		}
		return ECheckBoxState::Unchecked;
	}

	FSlateColor GetAssetTextColor() const
	{
		static const FName InvertedForeground("InvertedForeground");
		return GetCheckState() == ECheckBoxState::Checked || CheckBox->IsHovered() ? FEditorStyle::GetSlateColor(InvertedForeground) : FSlateColor::UseForeground();
	}

	TSharedRef<SWidget> HandleGetMenuContent()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, nullptr);

		MenuBuilder.BeginSection("AssetActions", LOCTEXT("AssetActionsSection", "Asset Actions"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ShowInContentBrowser", "Show In Content Browser"),
				LOCTEXT("ShowInContentBrowser_ToolTip", "Show this asset in the content browser."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "PropertyWindow.Button_Browse"),
				FUIAction(FExecuteAction::CreateSP(this, &SAssetShortcut::HandleShowInContentBrowser)));
		}
		MenuBuilder.EndSection();

		if (bMultipleAssetsExist)
		{
			MenuBuilder.BeginSection("AssetSelection", LOCTEXT("AssetSelectionSection", "Select Asset"));
			{
				FAssetPickerConfig AssetPickerConfig;

				UClass* FilterClass = AssetFamily->GetAssetFamilyClass(AssetData.GetClass());
				if (FilterClass != nullptr)
				{
					AssetPickerConfig.Filter.ClassNames.Add(FilterClass->GetFName());
					AssetPickerConfig.Filter.bRecursiveClasses = true;
				}

				AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SAssetShortcut::HandleAssetSelectedFromPicker);
				AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SAssetShortcut::HandleFilterAsset);
				AssetPickerConfig.bAllowNullSelection = false;
				AssetPickerConfig.ThumbnailLabel = EThumbnailLabel::ClassName;
				AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

				MenuBuilder.AddWidget(
					SNew(SBox)
					.WidthOverride(300)
					.HeightOverride(600)
					[
						ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
					],
					FText(), true);
			}
			MenuBuilder.EndSection();
		}
		
		return MenuBuilder.MakeWidget();
	}

	void HandleAssetSelectedFromPicker(const class FAssetData& InAssetData)
	{
		TArray<UObject*> Assets;
		Assets.Add(InAssetData.GetAsset());
		FAssetEditorManager::Get().OpenEditorForAssets(Assets);
	}

	bool HandleFilterAsset(const class FAssetData& InAssetData)
	{
		return !AssetFamily->IsAssetCompatible(InAssetData);
	}

	EVisibility GetButtonVisibility() const
	{
		return AssetData.IsValid() || bMultipleAssetsExist ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetComboVisibility() const
	{
		return bMultipleAssetsExist && AssetData.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	void HandleFilesLoaded()
	{
		TArray<FAssetData> Assets;
		AssetFamily->FindAssetsOfType(AssetData.GetClass(), Assets);
		bMultipleAssetsExist = Assets.Num() > 1;
	}

	void HandleAssetRemoved(const FAssetData& InAssetData)
	{
		if (AssetFamily->IsAssetCompatible(InAssetData))
		{
			TArray<FAssetData> Assets;
			AssetFamily->FindAssetsOfType(AssetData.GetClass(), Assets);
			bMultipleAssetsExist = Assets.Num() > 1;
		}
	}

	void HandleAssetAdded(const FAssetData& InAssetData)
	{
		if (AssetFamily->IsAssetCompatible(InAssetData))
		{
			TArray<FAssetData> Assets;
			AssetFamily->FindAssetsOfType(AssetData.GetClass(), Assets);
			bMultipleAssetsExist = Assets.Num() > 1;
		}
	}

	void HandleShowInContentBrowser()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FAssetData> Assets;
		Assets.Add(AssetData);
		ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
	}

	void HandleAssetOpened(UObject* InAsset)
	{
		RefreshAsset();
	}

	EVisibility GetThumbnailVisibility() const
	{
		return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	EVisibility GetSmallThumbnailVisibility() const
	{
		return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* GetDirtyImage() const
	{
		return bPackageDirty ? AssetDirtyBrush : nullptr;
	}

	void RefreshAsset()
	{
		// if this is the asset being edited by our hosting asset editor, don't switch it
		bool bAssetBeingEdited = false;
		const TArray<UObject*>* Objects = HostingApp.Pin()->GetObjectsCurrentlyBeingEdited();
		if (Objects != nullptr)
		{
			for (UObject* Object : *Objects)
			{
				if (FAssetData(Object) == AssetData)
				{
					bAssetBeingEdited = true;
					break;
				}
			}
		}

		// switch to new asset if needed
		FAssetData NewAssetData = AssetFamily->FindAssetOfType(AssetData.GetClass());
		if (!bAssetBeingEdited && NewAssetData.IsValid() && NewAssetData != AssetData)
		{
			AssetData = NewAssetData;

			// regenerate thumbnail
			AssetThumbnail = MakeShareable(new FAssetThumbnail(AssetData, AssetShortcutConstants::ThumbnailSize, AssetShortcutConstants::ThumbnailSize, ThumbnailPoolPtr.Pin()));
			AssetThumbnailSmall = MakeShareable(new FAssetThumbnail(AssetData, AssetShortcutConstants::ThumbnailSizeSmall, AssetShortcutConstants::ThumbnailSizeSmall, ThumbnailPoolPtr.Pin()));

			ThumbnailBox->SetContent(AssetThumbnail->MakeThumbnailWidget());
			ThumbnailSmallBox->SetContent(AssetThumbnailSmall->MakeThumbnailWidget());
		}
	}

	EActiveTimerReturnType HandleRefreshDirtyState(double InCurrentTime, float InDeltaTime)
	{
		if (AssetData.IsAssetLoaded())
		{
			if (!AssetPackage.IsValid())
			{
				AssetPackage = AssetData.GetPackage();
			}

			if (AssetPackage.IsValid())
			{
				bPackageDirty = AssetPackage->IsDirty();
			}
		}

		return EActiveTimerReturnType::Continue;
	}

private:
	/** The current asset data for this widget */
	FAssetData AssetData;

	/** Cache the package of the object for checking dirty state */
	TWeakObjectPtr<UPackage> AssetPackage;

	/** Timer handle used to updating dirty state */
	TSharedPtr<class FActiveTimerHandle> DirtyStateTimerHandle;

	/** The asset family we are working with */
	TSharedPtr<class IAssetFamily> AssetFamily;

	/** Our asset thumbnails */
	TSharedPtr<FAssetThumbnail> AssetThumbnail;
	TSharedPtr<FAssetThumbnail> AssetThumbnailSmall;

	/** Thumbnail widget containers */
	TSharedPtr<SBox> ThumbnailBox;
	TSharedPtr<SBox> ThumbnailSmallBox;

	/** The asset editor we are embedded in */
	TWeakPtr<class FWorkflowCentricApplication> HostingApp;

	/** Thumbnail pool */
	TWeakPtr<FAssetThumbnailPool> ThumbnailPoolPtr;

	/** Check box */
	TSharedPtr<SCheckBox> CheckBox;

	/** Cached dirty brush */
	const FSlateBrush* AssetDirtyBrush;

	/** Whether there are multiple (>1) of this asset type in existence */
	bool bMultipleAssetsExist;

	/** Cache the package's dirty state */
	bool bPackageDirty;
};

void SAssetFamilyShortcutBar::Construct(const FArguments& InArgs, const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IAssetFamily>& InAssetFamily)
{
	ThumbnailPool = MakeShareable(new FAssetThumbnailPool(16, false));

	TSharedRef<SUniformGridPanel> Panel = SNew(SUniformGridPanel);

	int32 ColumnIndex = 0;
	TArray<UClass*> AssetTypes;
	InAssetFamily->GetAssetTypes(AssetTypes);

	for (UClass* Class : AssetTypes)
	{
		FAssetData AssetData = InAssetFamily->FindAssetOfType(Class);
		Panel->AddSlot(ColumnIndex, 0)
		[
			SNew(SAssetShortcut, InHostingApp, InAssetFamily, AssetData, ThumbnailPool.ToSharedRef())
		];

		ColumnIndex++;
	}

	ChildSlot
	[
		Panel
	];
}

#undef LOCTEXT_NAMESPACE