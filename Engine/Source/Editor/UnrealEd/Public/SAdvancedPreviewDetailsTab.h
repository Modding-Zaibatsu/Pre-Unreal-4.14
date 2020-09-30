// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NotifyHook.h"

class IDetailsView;
class FAdvancedPreviewScene;
class UAssetViewerSettings;
class UEditorPerProjectUserSettings;

class UNREALED_API SAdvancedPreviewDetailsTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAdvancedPreviewDetailsTab)
	{}

	SLATE_END_ARGS()

public:
	/** **/
	SAdvancedPreviewDetailsTab();
	~SAdvancedPreviewDetailsTab();

	/** SWidget functions */
	void Construct(const FArguments& InArgs, const TSharedRef<FAdvancedPreviewScene>& InPreviewScene);

	void Refresh();

protected:
	void CreateSettingsView();
	void ComboBoxSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type /*SelectInfo*/);
	void UpdateSettingsView();
	void UpdateProfileNames();
	FReply AddProfileButtonClick();
	FReply RemoveProfileButtonClick();
protected:
	void OnAssetViewerSettingsRefresh(const FName& InPropertyName);
protected:
	/** Property viewing widget */
	TSharedPtr<IDetailsView> SettingsView;
	TSharedPtr<STextComboBox> ProfileComboBox;
	TWeakPtr<FAdvancedPreviewScene> PreviewScenePtr;
	UAssetViewerSettings* DefaultSettings;

	TArray<TSharedPtr<FString>> ProfileNames;
	int32 ProfileIndex;

	FDelegateHandle RefreshDelegate;
	FDelegateHandle AddRemoveProfileDelegate;

	UEditorPerProjectUserSettings* PerProjectSettings;
};
