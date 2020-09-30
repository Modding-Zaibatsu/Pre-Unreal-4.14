// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneCameraShakeTrack.h"
#include "MovieSceneCameraShakeSection.h"
#include "ISequencerSection.h"
#include "ISectionLayoutBuilder.h"
#include "MovieSceneTrackEditor.h"
#include "CameraShakeTrackEditor.h"
#include "AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "SequencerUtilities.h"

#define LOCTEXT_NAMESPACE "FCameraShakeTrackEditor"

static const FName ParentClassTagName(TEXT("ParentClass"));
static const FString CameraShakeClassPath(TEXT("Class'/Script/Engine.CameraShake'"));


class FCameraShakeSection : public ISequencerSection
{
public:
	FCameraShakeSection(UMovieSceneSection& InSection)
		: Section(InSection)
	{ }

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override
	{
		return &Section;
	}

	virtual FText GetDisplayName() const override
	{
		return LOCTEXT("CameraShakeSection", "Camera Shake");
	}

	virtual FText GetSectionTitle() const override
	{
		UMovieSceneCameraShakeSection const* const ShakeSection = Cast<UMovieSceneCameraShakeSection>(&Section);
		UClass const* const Shake = ShakeSection ? ShakeSection->GetCameraShakeClass() : nullptr;
		if (Shake)
		{
			return FText::FromString(Shake->GetName());
		}
		return LOCTEXT("NoCameraShakeSection", "No Camera Shake");
	}

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override
	{
// 		UMovieSceneCameraShakeSection* PathSection = Cast<UMovieSceneCameraShakeSection>(&Section);
// 		LayoutBuilder.AddKeyArea("Timing", LOCTEXT("TimingArea", "Timing"), MakeShareable( new FFloatCurveKeyArea ( &PathSection->GetTimingCurve( ), PathSection ) ) );
	}

	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override
	{
		return InPainter.PaintSectionBackground();
	}

private:

	/** The section we are visualizing */
	UMovieSceneSection& Section;
};


FCameraShakeTrackEditor::FCameraShakeTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor( InSequencer ) 
{ 
}


TSharedRef<ISequencerTrackEditor> FCameraShakeTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FCameraShakeTrackEditor(InSequencer));
}


bool FCameraShakeTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneCameraShakeTrack::StaticClass();
}


TSharedRef<ISequencerSection> FCameraShakeTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShareable(new FCameraShakeSection(SectionObject));
}


void FCameraShakeTrackEditor::AddKey(const FGuid& ObjectGuid)
{
	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Collect a full list of assets with the specified class
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(UCameraShake::StaticClass()->GetFName(), AssetDataList);

	if (AssetDataList.Num())
	{
		TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (Parent.IsValid())
		{
			FSlateApplication::Get().PushMenu(
				Parent.ToSharedRef(),
				FWidgetPath(),
				BuildCameraShakeSubMenu(ObjectGuid),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
				);
		}
	}
}

bool FCameraShakeTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	if (TargetObjectGuid.IsValid())
	{
		UBlueprint* const SelectedObject = dynamic_cast<UBlueprint*>(Asset);
		if (SelectedObject && SelectedObject->GeneratedClass && SelectedObject->GeneratedClass->IsChildOf(UCameraShake::StaticClass()))
		{
			TSubclassOf<UCameraShake> const ShakeClass = *(SelectedObject->GeneratedClass);

			TArray<TWeakObjectPtr<UObject>> OutObjects;
			GetSequencer()->GetRuntimeObjects(GetSequencer()->GetFocusedMovieSceneSequenceInstance(), TargetObjectGuid, OutObjects);
			AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FCameraShakeTrackEditor::AddKeyInternal, OutObjects, ShakeClass));

			return true;
		}
	}

	return false;
}

void FCameraShakeTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	// only offer this track if we can find a camera component
	UCameraComponent const* const CamComponent = AcquireCameraComponentFromObjectGuid(ObjectBinding);
	if (CamComponent)
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

		// Load the asset registry module
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		// Collect a full list of assets with the specified class
		TArray<FAssetData> AssetDataList;
		{
			FARFilter Filter;
			Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());

			TMultiMap<FName, FString> TagsAndValuesFilter;
			TagsAndValuesFilter.Add(ParentClassTagName, CameraShakeClassPath);
			Filter.TagsAndValues = TagsAndValuesFilter;

			AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);
		}

		if (AssetDataList.Num())
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("AddCameraShake", "Camera Shake"), NSLOCTEXT("Sequencer", "AddCameraShakeTooltip", "Adds an additive camera shake track."),
				FNewMenuDelegate::CreateRaw(this, &FCameraShakeTrackEditor::AddCameraShakeSubMenu, ObjectBinding)
				);
		}
	}
}

TSharedRef<SWidget> FCameraShakeTrackEditor::BuildCameraShakeSubMenu(FGuid ObjectBinding)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	AddCameraShakeSubMenu(MenuBuilder, ObjectBinding);

	return MenuBuilder.MakeWidget();
}


void FCameraShakeTrackEditor::AddCameraShakeSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding)
{
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FCameraShakeTrackEditor::OnCameraShakeAssetSelected, ObjectBinding);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
		AssetPickerConfig.Filter.TagsAndValues.Add(ParentClassTagName, CameraShakeClassPath);
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TSharedPtr<SBox> MenuEntry = SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

	MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
}

TSharedPtr<SWidget> FCameraShakeTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	// Create a container edit box
	return SNew(SHorizontalBox)

	// Add the camera shake combo box
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		FSequencerUtilities::MakeAddButton(LOCTEXT("AddCameraShake", "Camera Shake"), FOnGetContent::CreateSP(this, &FCameraShakeTrackEditor::BuildCameraShakeSubMenu, ObjectBinding), Params.NodeIsHovered)
	];
}


void FCameraShakeTrackEditor::OnCameraShakeAssetSelected(const FAssetData& AssetData, FGuid ObjectBinding)
{
	FSlateApplication::Get().DismissAllMenus();

	UBlueprint* const SelectedObject = dynamic_cast<UBlueprint*>(AssetData.GetAsset());
	if (SelectedObject && SelectedObject->GeneratedClass && SelectedObject->GeneratedClass->IsChildOf(UCameraShake::StaticClass()))
	{
		TSubclassOf<UCameraShake> const ShakeClass = *(SelectedObject->GeneratedClass);

		TArray<TWeakObjectPtr<UObject>> OutObjects;

		GetSequencer()->GetRuntimeObjects(GetSequencer()->GetFocusedMovieSceneSequenceInstance(), ObjectBinding, OutObjects);
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FCameraShakeTrackEditor::AddKeyInternal, OutObjects, ShakeClass));
	}
}


bool FCameraShakeTrackEditor::AddKeyInternal(float KeyTime, const TArray<TWeakObjectPtr<UObject>> Objects, TSubclassOf<UCameraShake> ShakeClass)
{
	bool bHandleCreated = false;
	bool bTrackCreated = false;
	bool bTrackModified = false;

	for (int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); ++ObjectIndex)
	{
		UObject* Object = Objects[ObjectIndex].Get();

		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
		FGuid ObjectHandle = HandleResult.Handle;
		bHandleCreated |= HandleResult.bWasCreated;
		if (ObjectHandle.IsValid())
		{
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieSceneCameraShakeTrack::StaticClass());
			UMovieSceneTrack* Track = TrackResult.Track;
			bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(Track))
			{
				Cast<UMovieSceneCameraShakeTrack>(Track)->AddNewCameraShake(KeyTime, ShakeClass);
				bTrackModified = true;
			}
		}
	}

	return bHandleCreated || bTrackCreated || bTrackModified;
}

UCameraComponent* FCameraShakeTrackEditor::AcquireCameraComponentFromObjectGuid(const FGuid& Guid)
{
	TArray<TWeakObjectPtr<UObject>> OutObjects;
	GetSequencer()->GetRuntimeObjects(GetSequencer()->GetFocusedMovieSceneSequenceInstance(), Guid, OutObjects);

	USkeleton* Skeleton = nullptr;
	for (int32 i = 0; i < OutObjects.Num(); ++i)
	{
		UObject* const Obj = OutObjects[i].Get();
		if (AActor* const Actor = Cast<AActor>(Obj))
		{
			UCameraComponent* const CameraComp = MovieSceneHelpers::CameraComponentFromActor(Actor);
			if (CameraComp)
			{
				return CameraComp;
			}
		}
		else if (UCameraComponent* const CameraComp = Cast<UCameraComponent>(Obj))
		{
			if (CameraComp->bIsActive)
			{
				return CameraComp;
			}
		}
	}

	return nullptr;
}


#undef LOCTEXT_NAMESPACE
