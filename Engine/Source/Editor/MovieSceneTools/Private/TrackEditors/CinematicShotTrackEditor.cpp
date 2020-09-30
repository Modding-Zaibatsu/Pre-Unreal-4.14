// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "ISequencerSection.h"
#include "MovieSceneTrack.h"
#include "MovieSceneCinematicShotTrack.h"
#include "MovieSceneCinematicShotSection.h"
#include "ScopedTransaction.h"
#include "ISequencerObjectChangeListener.h"
#include "IKeyArea.h"
#include "MovieSceneTrackEditor.h"
#include "MovieSceneToolHelpers.h"
#include "CinematicShotTrackEditor.h"
#include "CommonMovieSceneTools.h"
#include "CinematicShotSection.h"
#include "SequencerUtilities.h"
#include "ContentBrowserModule.h"
#include "MovieSceneCaptureSettings.h"
#include "MovieSceneCapture.h"
#include "AutomatedLevelSequenceCapture.h"
#include "MovieSceneCaptureModule.h"
#include "LevelSequence.h"
#include "AssetToolsModule.h"
#include "TrackEditorThumbnailPool.h"
#include "MovieSceneToolsProjectSettings.h"


#define LOCTEXT_NAMESPACE "FCinematicShotTrackEditor"


/* FCinematicShotTrackEditor structors
 *****************************************************************************/

FCinematicShotTrackEditor::FCinematicShotTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer) 
{
	ThumbnailPool = MakeShareable(new FTrackEditorThumbnailPool(InSequencer));
}


TSharedRef<ISequencerTrackEditor> FCinematicShotTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FCinematicShotTrackEditor(InSequencer));
}


void FCinematicShotTrackEditor::OnInitialize()
{
	OnCameraCutHandle = GetSequencer()->OnCameraCut().AddSP(this, &FCinematicShotTrackEditor::OnUpdateCameraCut);
}


void FCinematicShotTrackEditor::OnRelease()
{
	if (OnCameraCutHandle.IsValid() && GetSequencer().IsValid())
	{
		GetSequencer()->OnCameraCut().Remove(OnCameraCutHandle);
	}
}


/* ISequencerTrackEditor interface
 *****************************************************************************/

void FCinematicShotTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	UMovieSceneSequence* RootMovieSceneSequence = GetSequencer()->GetRootMovieSceneSequence();

	if ((RootMovieSceneSequence == nullptr) || (RootMovieSceneSequence->GetClass()->GetName() != TEXT("LevelSequence")))
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddCinematicShotTrack", "Shot Track"),
		LOCTEXT("AddCinematicShotTooltip", "Adds a shot track."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.CinematicShot"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FCinematicShotTrackEditor::HandleAddCinematicShotTrackMenuEntryExecute),
			FCanExecuteAction::CreateRaw(this, &FCinematicShotTrackEditor::HandleAddCinematicShotTrackMenuEntryCanExecute)
		)
	);
}


TSharedPtr<SWidget> FCinematicShotTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	// Create a container edit box
	return SNew(SHorizontalBox)

	// Add the camera combo box
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		FSequencerUtilities::MakeAddButton(LOCTEXT("CinematicShotText", "Shot"), FOnGetContent::CreateSP(this, &FCinematicShotTrackEditor::HandleAddCinematicShotComboButtonGetMenuContent), Params.NodeIsHovered)
	]

	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Right)
	.AutoWidth()
	.Padding(4, 0, 0, 0)
	[
		SNew(SCheckBox)
		.IsFocusable(false)
		.IsChecked(this, &FCinematicShotTrackEditor::AreShotsLocked)
		.OnCheckStateChanged(this, &FCinematicShotTrackEditor::OnLockShotsClicked)
		.ToolTipText(this, &FCinematicShotTrackEditor::GetLockShotsToolTip)
		.ForegroundColor(FLinearColor::White)
		.CheckedImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
		.CheckedHoveredImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
		.CheckedPressedImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
		.UncheckedImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
		.UncheckedHoveredImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
		.UncheckedPressedImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
	];
}


TSharedRef<ISequencerSection> FCinematicShotTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));

	return MakeShareable(new FCinematicShotSection(GetSequencer(), ThumbnailPool, SectionObject, SharedThis(this)));
}


bool FCinematicShotTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	UMovieSceneSequence* Sequence = Cast<UMovieSceneSequence>(Asset);

	if (Sequence == nullptr)
	{
		return false;
	}


	//@todo If there's already a subscenes track, allow that track to handle this asset
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene != nullptr && FocusedMovieScene->FindMasterTrack<UMovieSceneSubTrack>() != nullptr)
	{
		return false;
	}

	if (CanAddSubSequence(*Sequence))
	{
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FCinematicShotTrackEditor::HandleSequenceAdded, Sequence));

		return true;
	}

	return false;
}


bool FCinematicShotTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneCinematicShotTrack::StaticClass());
}


void FCinematicShotTrackEditor::Tick(float DeltaTime)
{
	TSharedPtr<ISequencer> SequencerPin = GetSequencer();
	if (!SequencerPin.IsValid())
	{
		return;
	}

	EMovieScenePlayerStatus::Type PlaybackState = SequencerPin->GetPlaybackStatus();

	if (FSlateThrottleManager::Get().IsAllowingExpensiveTasks() && PlaybackState != EMovieScenePlayerStatus::Playing && PlaybackState != EMovieScenePlayerStatus::Scrubbing)
	{
		SequencerPin->EnterSilentMode();

		float SavedTime = SequencerPin->GetGlobalTime();

		if (DeltaTime > 0.f && ThumbnailPool->DrawThumbnails())
		{
			SequencerPin->SetGlobalTimeDirectly(SavedTime);
		}

		SequencerPin->ExitSilentMode();
	}
}


void FCinematicShotTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "ImportEDL", "Import EDL..." ),
		NSLOCTEXT( "Sequencer", "ImportEDLTooltip", "Import Edit Decision List (EDL) for non-linear editors." ),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateRaw(this, &FCinematicShotTrackEditor::ImportEDL )));

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "ExportEDL", "Export EDL..." ),
		NSLOCTEXT( "Sequencer", "ExportEDLTooltip", "Export Edit Decision List (EDL) for non-linear editors." ),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateRaw(this, &FCinematicShotTrackEditor::ExportEDL )));
}


const FSlateBrush* FCinematicShotTrackEditor::GetIconBrush() const
{
	return FEditorStyle::GetBrush("Sequencer.Tracks.CinematicShot");
}


UMovieSceneSubSection* FCinematicShotTrackEditor::CreateShotInternal(FString& NewShotName, float NewShotStartTime, UMovieSceneCinematicShotSection* ShotToDuplicate, const bool& bInsertShot)
{
	FString NewShotPath = MovieSceneToolHelpers::GenerateNewShotPath(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), NewShotName);

	// Create a new level sequence asset with the appropriate name
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UObject* NewAsset = nullptr;
	for (TObjectIterator<UClass> It ; It ; ++It)
	{
		UClass* CurrentClass = *It;
		if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
			if (Factory->CanCreateNew() && Factory->ImportPriority >= 0 && Factory->SupportedClass == ULevelSequence::StaticClass())
			{
				if (ShotToDuplicate != nullptr)
				{
					NewAsset = AssetTools.DuplicateAssetWithDialog(NewShotName, NewShotPath, ShotToDuplicate->GetSequence());
				}
				else
				{
					NewAsset = AssetTools.CreateAssetWithDialog(NewShotName, NewShotPath, ULevelSequence::StaticClass(), Factory);
				}
				break;
			}
		}
	}

	if (NewAsset == nullptr)
	{
		return nullptr;
	}

	UMovieSceneSequence* NewSequence = Cast<UMovieSceneSequence>(NewAsset);

	float Duration = NewSequence->GetMovieScene()->GetPlaybackRange().Size<float>();
	if (ShotToDuplicate != nullptr)
	{
		Duration = ShotToDuplicate->GetEndTime() - ShotToDuplicate->GetStartTime();
	}

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();

	// Cut any existing overlapping sections on the top row
	for (auto ShotSection : CinematicShotTrack->GetAllSections())
	{
		if (ShotSection->GetRowIndex() == 0 && NewShotStartTime > ShotSection->GetStartTime() && NewShotStartTime < ShotSection->GetEndTime())
		{
			ShotSection->SetEndTime(NewShotStartTime);
		}
	}

	// Create a cinematic shot section and set the time to be at the end of this shot. 
	UMovieSceneSubSection* NewSection = CinematicShotTrack->AddSequence(NewSequence, NewShotStartTime, Duration, bInsertShot);	
	return NewSection;
}


void FCinematicShotTrackEditor::InsertShotAtCurrentTime()
{
	const FScopedTransaction Transaction(LOCTEXT("InsertShot_Transaction", "Insert Shot"));

	float NewShotStartTime = GetSequencer()->GetGlobalTime();

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();
	FString NewShotName = MovieSceneToolHelpers::GenerateNewShotName(CinematicShotTrack->GetAllSections(), NewShotStartTime);

	CreateShotInternal(NewShotName, NewShotStartTime);

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


void FCinematicShotTrackEditor::InsertShot(UMovieSceneCinematicShotSection* Section)
{
	const FScopedTransaction Transaction(LOCTEXT("InsertShot_Transaction", "Insert Shot"));

	// Start this new shot after the current shot
	float NewShotStartTime = Section->GetEndTime();

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();
	FString NewShotName = MovieSceneToolHelpers::GenerateNewShotName(CinematicShotTrack->GetAllSections(), NewShotStartTime);

	CreateShotInternal(NewShotName, NewShotStartTime);

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


void FCinematicShotTrackEditor::InsertFillerAtCurrentTime()
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	const FScopedTransaction Transaction(LOCTEXT("InsertFiller_Transaction", "Insert Filler"));

	float NewShotStartTime = GetSequencer()->GetGlobalTime();

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();

	float Duration = ProjectSettings->DefaultDuration;
	const bool bInsertShot = true;

	UMovieSceneSequence* NullSequence = nullptr;

	UMovieSceneSubSection* NewSection = CinematicShotTrack->AddSequence(NullSequence, NewShotStartTime, Duration, bInsertShot);	

	UMovieSceneCinematicShotSection* NewCinematicShotSection = Cast<UMovieSceneCinematicShotSection>(NewSection);

	NewCinematicShotSection->SetShotDisplayName(FText(LOCTEXT("Filler", "Filler")));

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


void FCinematicShotTrackEditor::DuplicateShot(UMovieSceneCinematicShotSection* Section)
{
	const FScopedTransaction Transaction(LOCTEXT("DuplicateShot_Transaction", "Duplicate Shot"));

	// Start this new shot after the current shot
	float NewShotStartTime = Section->GetEndTime();

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();
	FString NewShotName = MovieSceneToolHelpers::GenerateNewShotName(CinematicShotTrack->GetAllSections(), NewShotStartTime);

	UMovieSceneSubSection* NewShot = CreateShotInternal(NewShotName, NewShotStartTime, Section);
	if (NewShot)
	{
		NewShot->SetEndTime(NewShot->GetStartTime() + Section->GetEndTime() - Section->GetStartTime());
		NewShot->StartOffset = Section->StartOffset;
		NewShot->TimeScale = Section->TimeScale;
		NewShot->PrerollTime = Section->PrerollTime;

		GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}


void FCinematicShotTrackEditor::RenderShot(UMovieSceneCinematicShotSection* Section)
{
	GetSequencer()->RenderMovie(Section);

}


void FCinematicShotTrackEditor::RenameShot(UMovieSceneCinematicShotSection* Section)
{
	//@todo
}


void FCinematicShotTrackEditor::NewTake(UMovieSceneCinematicShotSection* Section)
{
	const FScopedTransaction Transaction(LOCTEXT("NewTake_Transaction", "New Take"));

	FString ShotPrefix;
	uint32 ShotNumber = INDEX_NONE;
	uint32 TakeNumber = INDEX_NONE;
	if (MovieSceneToolHelpers::ParseShotName(Section->GetShotDisplayName().ToString(), ShotPrefix, ShotNumber, TakeNumber))
	{
		TArray<uint32> TakeNumbers;
		uint32 CurrentTakeNumber;
		MovieSceneToolHelpers::GatherTakes(Section, TakeNumbers, CurrentTakeNumber);
		uint32 NewTakeNumber = CurrentTakeNumber;
		if (TakeNumbers.Num() > 0)
		{
			NewTakeNumber = TakeNumbers[TakeNumbers.Num()-1] + 1;
		}

		FString NewShotName = MovieSceneToolHelpers::ComposeShotName(ShotPrefix, ShotNumber, NewTakeNumber);

		float NewShotStartTime = Section->GetStartTime();
		float NewShotEndTime = Section->GetEndTime();
		float NewShotStartOffset = Section->StartOffset;
		float NewShotTimeScale = Section->TimeScale;
		float NewShotPrerollTime = Section->PrerollTime;

		const bool bInsertShot = false;
		UMovieSceneSubSection* NewShot = CreateShotInternal(NewShotName, NewShotStartTime, Section, bInsertShot);

		if (NewShot)
		{
			UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();
			CinematicShotTrack->RemoveSection(*Section);

			NewShot->SetStartTime(NewShotStartTime);
			NewShot->SetEndTime(NewShotEndTime);
			NewShot->StartOffset = NewShotStartOffset;
			NewShot->TimeScale = NewShotTimeScale;
			NewShot->PrerollTime = NewShotPrerollTime;

			GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
		}
	}
}


void FCinematicShotTrackEditor::SwitchTake(UMovieSceneCinematicShotSection* Section, uint32 TakeNumber)
{
	const FScopedTransaction Transaction(LOCTEXT("SwitchTake_Transaction", "Switch Take"));

	UObject* TakeObject = MovieSceneToolHelpers::GetTake(Section, TakeNumber);

	if (TakeObject && TakeObject->IsA(UMovieSceneSequence::StaticClass()))
	{
		UMovieSceneSequence* MovieSceneSequence = CastChecked<UMovieSceneSequence>(TakeObject);

		UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();
		
		float NewShotStartTime = Section->GetStartTime();
		float NewShotEndTime = Section->GetEndTime();
		float NewShotStartOffset = Section->StartOffset;
		float NewShotTimeScale = Section->TimeScale;
		float NewShotPrerollTime = Section->PrerollTime;

		const bool bInsertSequence = false;
		const float Duration = NewShotEndTime - NewShotStartTime;
		UMovieSceneSubSection* NewShot = CinematicShotTrack->AddSequence(MovieSceneSequence, NewShotStartTime, Duration, bInsertSequence);	

		if (NewShot != nullptr)
		{
			CinematicShotTrack->RemoveSection(*Section);

			NewShot->SetStartTime(NewShotStartTime);
			NewShot->SetEndTime(NewShotEndTime);
			NewShot->StartOffset = NewShotStartOffset;
			NewShot->TimeScale = NewShotTimeScale;
			NewShot->PrerollTime = NewShotPrerollTime;

			GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
		}
	}

}


/* FCinematicShotTrackEditor callbacks
 *****************************************************************************/

bool FCinematicShotTrackEditor::HandleAddCinematicShotTrackMenuEntryCanExecute() const
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	return ((FocusedMovieScene != nullptr) && (FocusedMovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>() == nullptr));
}


void FCinematicShotTrackEditor::HandleAddCinematicShotTrackMenuEntryExecute()
{
	FindOrCreateCinematicShotTrack();
}


TSharedRef<SWidget> FCinematicShotTrackEditor::HandleAddCinematicShotComboButtonGetMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
			LOCTEXT("InsertShot", "Insert Shot"),
			LOCTEXT("InsertShotTooltip", "Insert new shot at current time"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FCinematicShotTrackEditor::InsertShotAtCurrentTime))
	);

	MenuBuilder.AddMenuEntry(
			LOCTEXT("InsertFiller", "Insert Filler"),
			LOCTEXT("InsertFillerTooltip", "Insert filler at current time"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FCinematicShotTrackEditor::InsertFillerAtCurrentTime))
	);

	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw( this, &FCinematicShotTrackEditor::HandleAddCinematicShotComboButtonMenuEntryExecute);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::Tile;
		AssetPickerConfig.Filter.ClassNames.Add(TEXT("LevelSequence"));
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TSharedPtr<SBox> MenuEntry = SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

	MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);

	return MenuBuilder.MakeWidget();
}


void FCinematicShotTrackEditor::HandleAddCinematicShotComboButtonMenuEntryExecute(const FAssetData& AssetData)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject && SelectedObject->IsA(UMovieSceneSequence::StaticClass()))
	{
		UMovieSceneSequence* MovieSceneSequence = CastChecked<UMovieSceneSequence>(AssetData.GetAsset());

		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FCinematicShotTrackEditor::AddKeyInternal, MovieSceneSequence) );
	}
}


bool FCinematicShotTrackEditor::AddKeyInternal(float KeyTime, UMovieSceneSequence* InMovieSceneSequence)
{	
	if (CanAddSubSequence(*InMovieSceneSequence))
	{
		UMovieSceneCinematicShotTrack* CinematicShotTrack = FindOrCreateCinematicShotTrack();
		float EndTime = InMovieSceneSequence->GetMovieScene()->GetPlaybackRange().Size<float>();
		CinematicShotTrack->AddSequence(InMovieSceneSequence, KeyTime, EndTime);

		return true;
	}

	return false;
}


UMovieSceneCinematicShotTrack* FCinematicShotTrackEditor::FindOrCreateCinematicShotTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	
	if (FocusedMovieScene == nullptr)
	{
		return nullptr;
	}

	UMovieSceneCinematicShotTrack* CinematicShotTrack = FocusedMovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (CinematicShotTrack != nullptr)
	{
		return CinematicShotTrack;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddCinematicShotTrack_Transaction", "Add Cinematic Shot Track"));
	FocusedMovieScene->Modify();

	auto NewTrack = FocusedMovieScene->AddMasterTrack<UMovieSceneCinematicShotTrack>();
	ensure(NewTrack);

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );

	return NewTrack;
}


ECheckBoxState FCinematicShotTrackEditor::AreShotsLocked() const
{
	if (GetSequencer()->IsPerspectiveViewportCameraCutEnabled())
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Unchecked;
	}
}


void FCinematicShotTrackEditor::OnLockShotsClicked(ECheckBoxState CheckBoxState)
{
	if (CheckBoxState == ECheckBoxState::Checked)
	{
		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
			if (LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() && LevelVC->GetViewMode() != VMI_Unknown)
			{
				LevelVC->SetActorLock(nullptr);
				LevelVC->bLockedCameraView = false;
				LevelVC->UpdateViewForLockedActor();
				LevelVC->Invalidate();
			}
		}
		GetSequencer()->SetPerspectiveViewportCameraCutEnabled(true);
	}
	else
	{
		GetSequencer()->UpdateCameraCut(nullptr, nullptr);
		GetSequencer()->SetPerspectiveViewportCameraCutEnabled(false);
	}

	GetSequencer()->SetGlobalTime(GetSequencer()->GetGlobalTime());
}


FText FCinematicShotTrackEditor::GetLockShotsToolTip() const
{
	return AreShotsLocked() == ECheckBoxState::Checked ?
		LOCTEXT("UnlockShots", "Unlock Viewport from Shots") :
		LOCTEXT("LockShots", "Lock Viewport to Shots");
}


bool FCinematicShotTrackEditor::CanAddSubSequence(const UMovieSceneSequence& Sequence) const
{
	// prevent adding ourselves and ensure we have a valid movie scene
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();

	if ((FocusedSequence == nullptr) || (FocusedSequence == &Sequence) || (FocusedSequence->GetMovieScene() == nullptr))
	{
		return false;
	}

	// ensure that the other sequence has a valid movie scene
	UMovieScene* SequenceMovieScene = Sequence.GetMovieScene();

	if (SequenceMovieScene == nullptr)
	{
		return false;
	}

	// make sure we are not contained in the other sequence (circular dependency)
	// @todo sequencer: this check is not sufficient (does not prevent circular dependencies of 2+ levels)
	UMovieSceneCinematicShotTrack* SequenceCinematicShotTrack = SequenceMovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();

	if (SequenceCinematicShotTrack == nullptr)
	{
		return true;
	}

	return !SequenceCinematicShotTrack->ContainsSequence(*FocusedSequence, true);
}


void FCinematicShotTrackEditor::OnUpdateCameraCut(UObject* CameraObject, bool bJumpCut)
{
	// Keep track of the camera when it switches so that the thumbnail can be drawn with the correct camera
	CinematicShotCamera = Cast<AActor>(CameraObject);
}


bool FCinematicShotTrackEditor::HandleSequenceAdded(float KeyTime, UMovieSceneSequence* Sequence)
{
	auto CinematicShotTrack = FindOrCreateCinematicShotTrack();
	float Duration = Sequence->GetMovieScene()->GetPlaybackRange().Size<float>();
	CinematicShotTrack->AddSequence(Sequence, KeyTime, Duration);

	return true;
}


void FCinematicShotTrackEditor::ImportEDL()
{
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return;
	}

	UMovieScene* MovieScene = FocusedSequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}

	UAutomatedLevelSequenceCapture* MovieSceneCapture = Cast<UAutomatedLevelSequenceCapture>(IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture());
	if (!MovieSceneCapture)
	{
		MovieSceneCapture = NewObject<UAutomatedLevelSequenceCapture>(GetTransientPackage(), UAutomatedLevelSequenceCapture::StaticClass(), NAME_None, RF_Transient);
		MovieSceneCapture->LoadFromConfig();
	}

	if (!MovieSceneCapture)
	{
		return;
	}

	const FMovieSceneCaptureSettings& Settings = MovieSceneCapture->GetSettings();
	FString SaveDirectory = FPaths::ConvertRelativePathToFull(Settings.OutputDirectory.Path);
	float FrameRate = Settings.FrameRate;

	if (MovieSceneToolHelpers::ShowImportEDLDialog(MovieScene, FrameRate, SaveDirectory))
	{
		GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemsChanged );
	}
}


void FCinematicShotTrackEditor::ExportEDL()
{
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
	if (!FocusedSequence)
	{
		return;
	}

	UMovieScene* MovieScene = FocusedSequence->GetMovieScene();
	if (!MovieScene)
	{
		return;
	}
		
	UAutomatedLevelSequenceCapture* MovieSceneCapture = Cast<UAutomatedLevelSequenceCapture>(IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture());
	if (!MovieSceneCapture)
	{
		MovieSceneCapture = NewObject<UAutomatedLevelSequenceCapture>(GetTransientPackage(), UAutomatedLevelSequenceCapture::StaticClass(), NAME_None, RF_Transient);
		MovieSceneCapture->LoadFromConfig();
	}

	if (!MovieSceneCapture)
	{
		return;
	}

	const FMovieSceneCaptureSettings& Settings = MovieSceneCapture->GetSettings();
	FString SaveDirectory = FPaths::ConvertRelativePathToFull(Settings.OutputDirectory.Path);
	float FrameRate = Settings.FrameRate;
	int32 HandleFrames = Settings.HandleFrames;

	MovieSceneToolHelpers::ShowExportEDLDialog(MovieScene, FrameRate, SaveDirectory, HandleFrames);
}


#undef LOCTEXT_NAMESPACE
