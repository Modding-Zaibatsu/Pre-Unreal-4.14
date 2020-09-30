// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneCameraAnimTrack.h"
#include "MovieSceneCameraAnimSection.h"
#include "ISequencerSection.h"
#include "ISectionLayoutBuilder.h"
#include "MovieSceneTrackEditor.h"
#include "CameraAnimTrackEditor.h"
#include "AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "SequencerUtilities.h"

#define LOCTEXT_NAMESPACE "FCameraAnimTrackEditor"


class FCameraAnimSection : public ISequencerSection
{
public:
	FCameraAnimSection(UMovieSceneSection& InSection)
		: Section( InSection )
	{ }

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override
	{ 
		return &Section;
	}

	virtual FText GetDisplayName() const override
	{ 
		return LOCTEXT("CameraAnimSection", "Camera Anim");
	}
	
	virtual FText GetSectionTitle() const override 
	{ 
		UMovieSceneCameraAnimSection const* const AnimSection = Cast<UMovieSceneCameraAnimSection>(&Section);
		UCameraAnim const* const Anim = AnimSection ? AnimSection->GetCameraAnim() : nullptr;
		if (Anim)
		{
			return FText::FromString(Anim->GetName());
		}
		return LOCTEXT("NoCameraAnimSection", "No Camera Anim");
	}

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
// 		UMovieSceneCameraAnimSection* PathSection = Cast<UMovieSceneCameraAnimSection>(&Section);
// 		LayoutBuilder.AddKeyArea("Timing", LOCTEXT("TimingArea", "Timing"), MakeShareable( new FFloatCurveKeyArea ( &PathSection->GetTimingCurve( ), PathSection ) ) );
	}

	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override 
	{
		return InPainter.PaintSectionBackground();
	}

private:
	/** The section we are visualizing */
	UMovieSceneSection& Section;
};


FCameraAnimTrackEditor::FCameraAnimTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor( InSequencer ) 
{ 
}


TSharedRef<ISequencerTrackEditor> FCameraAnimTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FCameraAnimTrackEditor(InSequencer));
}


bool FCameraAnimTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneCameraAnimTrack::StaticClass();
}


TSharedRef<ISequencerSection> FCameraAnimTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	return MakeShareable(new FCameraAnimSection(SectionObject));
}


void FCameraAnimTrackEditor::AddKey(const FGuid& ObjectGuid)
{
	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Collect a full list of assets with the specified class
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(UCameraAnim::StaticClass()->GetFName(), AssetDataList);

	if (AssetDataList.Num())
	{
		TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (Parent.IsValid())
		{
			FSlateApplication::Get().PushMenu(
				Parent.ToSharedRef(),
				FWidgetPath(),
				BuildCameraAnimSubMenu(ObjectGuid),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
				);
		}
	}
}


bool FCameraAnimTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	UCameraAnim* const CameraAnim = Cast<UCameraAnim>(Asset);
	if (CameraAnim)
	{
		if (TargetObjectGuid.IsValid())
		{
			TArray<TWeakObjectPtr<UObject>> OutObjects;

			GetSequencer()->GetRuntimeObjects(GetSequencer()->GetFocusedMovieSceneSequenceInstance(), TargetObjectGuid, OutObjects);
			AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FCameraAnimTrackEditor::AddKeyInternal, OutObjects, CameraAnim));

			return true;
		}
	}

	return false;
}



void FCameraAnimTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
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
		AssetRegistryModule.Get().GetAssetsByClass(UCameraAnim::StaticClass()->GetFName(), AssetDataList);

		if (AssetDataList.Num())
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("AddCameraAnim", "Camera Anim"), NSLOCTEXT("Sequencer", "AddCameraAnimTooltip", "Adds an additive camera animation track."),
				FNewMenuDelegate::CreateRaw(this, &FCameraAnimTrackEditor::AddCameraAnimSubMenu, ObjectBinding)
				);
		}
	}
}

TSharedRef<SWidget> FCameraAnimTrackEditor::BuildCameraAnimSubMenu(FGuid ObjectBinding)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	AddCameraAnimSubMenu(MenuBuilder, ObjectBinding);

	return MenuBuilder.MakeWidget();
}


void FCameraAnimTrackEditor::AddCameraAnimSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding)
{
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FCameraAnimTrackEditor::OnCameraAnimAssetSelected, ObjectBinding);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.Filter.ClassNames.Add(UCameraAnim::StaticClass()->GetFName());
//		AssetPickerConfig.Filter.TagsAndValues.Add(TEXT("Skeleton"), FAssetData(Skeleton).GetExportTextName());
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

TSharedPtr<SWidget> FCameraAnimTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	// Create a container edit box
	return SNew(SHorizontalBox)

	// Add the camera anim combo box
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		FSequencerUtilities::MakeAddButton(LOCTEXT("AddCameraAnim", "Camera Anim"), FOnGetContent::CreateSP(this, &FCameraAnimTrackEditor::BuildCameraAnimSubMenu, ObjectBinding), Params.NodeIsHovered)
	];
}

void FCameraAnimTrackEditor::OnCameraAnimAssetSelected(const FAssetData& AssetData, FGuid ObjectBinding)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject && SelectedObject->IsA(UCameraAnim::StaticClass()))
	{
		UCameraAnim* const CameraAnim = CastChecked<UCameraAnim>(AssetData.GetAsset());

		TArray<TWeakObjectPtr<UObject>> OutObjects;

		GetSequencer()->GetRuntimeObjects(GetSequencer()->GetFocusedMovieSceneSequenceInstance(), ObjectBinding, OutObjects);
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FCameraAnimTrackEditor::AddKeyInternal, OutObjects, CameraAnim));
	}
}



bool FCameraAnimTrackEditor::AddKeyInternal(float KeyTime, const TArray<TWeakObjectPtr<UObject>> Objects, UCameraAnim* CameraAnim)
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
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieSceneCameraAnimTrack::StaticClass());
			UMovieSceneTrack* Track = TrackResult.Track;
			bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(Track))
			{
				Cast<UMovieSceneCameraAnimTrack>(Track)->AddNewCameraAnim(KeyTime, CameraAnim);
				bTrackModified = true;
			}
		}
	}

	return bHandleCreated || bTrackCreated || bTrackModified;
}


UCameraComponent* FCameraAnimTrackEditor::AcquireCameraComponentFromObjectGuid(const FGuid& Guid)
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
