// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTrackEditor.h"
#include "IKeyframeSection.h"
#include "ISequencerObjectChangeListener.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneSection.h"

class IPropertyHandle;
class FPropertyChangedParams;

/**
 * A base class for track editors that edit tracks which contain sections implementing IKeyframeSection.
  */
template<typename TrackType, typename SectionType, typename KeyDataType>
class FKeyframeTrackEditor : public FMovieSceneTrackEditor
{

public:
	/**
	* Constructor
	*
	* @param InSequencer The sequencer instance to be used by this tool
	*/
	FKeyframeTrackEditor( TSharedRef<ISequencer> InSequencer )
		: FMovieSceneTrackEditor( InSequencer )
	{ }

	/** Virtual destructor. */
	~FKeyframeTrackEditor() { }

public:

	// ISequencerTrackEditor interface

	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override
	{
		return Type == TrackType::StaticClass();
	}


	virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override
	{
		MenuBuilder.AddSubMenu(
			NSLOCTEXT("KeyframeTrackEditor", "TrackDefaults", "Track Defaults"),
			NSLOCTEXT("KeyframeTrackEditor", "TrackDefaultsTooltip", "Track default value operations."),
			FNewMenuDelegate::CreateSP(this, &FKeyframeTrackEditor::AddTrackDefaultsItems, Track));
	}

protected:

	/*
	 * Adds keys to the specified object.  This may also add tracks and sections depending on the options specified. 
	 *
	 * @param ObjectsToKey An array of objects to add keyframes to.
	 * @param KeyTime The time to add keys.
	 * @param NewKeys The new keys to add.
	 * @param DefaultKeys Extra keys with default values which shouldn't be added directly, but are needed to set correct
	 *        default values when adding single channel keys for multi-channel tracks like vectors.
	 * @param KeyParams The parameters to control keyframing behavior.
	 * @param TrackClass The class of track which should be created if specified in the parameters.
	 * @param PropertyName The name of the property to add keys for.
	 * @param OnInitializeNewTrack A delegate which allows for custom initialization for new tracks.  This is called after the 
	 *        track is created, but before any sections or keys have been added.
	 * @return Whether or not a handle guid or track was created. Note this does not return true if keys were added or modified.
	 */
	bool AddKeysToObjects(
		TArray<UObject*> ObjectsToKey, float KeyTime,
		const TArray<KeyDataType>& NewKeys, const TArray<KeyDataType>& DefaultKeys,
		ESequencerKeyMode KeyMode, TSubclassOf<UMovieSceneTrack> TrackClass, FName PropertyName,
		TFunction<void(TrackType*)> OnInitializeNewTrack)
	{
		bool bHandleCreated = false;
		bool bTrackCreated = false;

		bool bCreateHandle =
			(KeyMode == ESequencerKeyMode::AutoKey && GetSequencer()->GetAutoKeyMode() == EAutoKeyMode::KeyAll) ||
			KeyMode == ESequencerKeyMode::ManualKey ||
			KeyMode == ESequencerKeyMode::ManualKeyForced;

		for ( UObject* Object : ObjectsToKey )
		{
			FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( Object, bCreateHandle );
			FGuid ObjectHandle = HandleResult.Handle;
			bHandleCreated = HandleResult.bWasCreated;

			if ( ObjectHandle.IsValid() )
			{
				bTrackCreated |= AddKeysToHandle( ObjectHandle, KeyTime, NewKeys, DefaultKeys, KeyMode, TrackClass, PropertyName, OnInitializeNewTrack );
			}
		}
		return bHandleCreated || bTrackCreated;
	}


private:

	virtual void ClearDefaults( UMovieSceneTrack* Track )
	{
		const FScopedTransaction Transaction(NSLOCTEXT("KeyframeTrackEditor", "ClearTrackDefaultsTransaction", "Clear track defaults"));
		for (UMovieSceneSection* Section : Track->GetAllSections())
		{
			Section->Modify();
			IKeyframeSection<KeyDataType>* KeyframeSection = CastChecked<SectionType>(Section);
			KeyframeSection->ClearDefaults();
		}
		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}

	void AddTrackDefaultsItems( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT( "KeyframeTrackEditor", "ClearDefaults", "Clear Defaults" ),
			NSLOCTEXT( "KeyframeTrackEditor", "ClearDefaultsToolTip", "Clear the current default values for this track." ),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &FKeyframeTrackEditor::ClearDefaults, Track ) ),
			NAME_None,
			EUserInterfaceActionType::Button );
	}

	/*
	 * Adds keys to the specified guid.  This may also add tracks and sections depending on the options specified. 
	 *
	 * @param ObjectsToKey An array of objects to add keyframes to.
	 * @param KeyTime The time to add keys.
	 * @param NewKeys The new keys to add.
	 * @param DefaultKeys Extra keys with default values which shouldn't be added directly, but are needed to set correct
	 *        default values when adding single channel keys for multi-channel tracks like vectors.
	 * @param KeyParams The parameters to control keyframing behavior.
	 * @param TrackClass The class of track which should be created if specified in the parameters.
	 * @param PropertyName The name of the property to add keys for.
	 * @param OnInitializeNewTrack A delegate which allows for custom initialization for new tracks.  This is called after the 
	 *        track is created, but before any sections or keys have been added.
	 * @return Whether or not a track was created. Note this does not return true if keys were added or modified.
	*/
	bool AddKeysToHandle(
		FGuid ObjectHandle, float KeyTime,
		const TArray<KeyDataType>& NewKeys, const TArray<KeyDataType>& DefaultKeys,
		ESequencerKeyMode KeyMode, TSubclassOf<UMovieSceneTrack> TrackClass, FName PropertyName,
		TFunction<void(TrackType*)> OnInitializeNewTrack)
	{
		bool bTrackCreated = false;
		bool bSectionCreated = false;

		bool bCreateTrack =
			(KeyMode == ESequencerKeyMode::AutoKey && GetSequencer()->GetAutoKeyMode() == EAutoKeyMode::KeyAll) ||
			KeyMode == ESequencerKeyMode::ManualKey ||
			KeyMode == ESequencerKeyMode::ManualKeyForced;

		// Try to find an existing Track, and if one doesn't exist check the key params and create one if requested.
		FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject( ObjectHandle, TrackClass, PropertyName, bCreateTrack );
		TrackType* Track = CastChecked<TrackType>( TrackResult.Track, ECastCheckedType::NullAllowed );
		bTrackCreated = TrackResult.bWasCreated;

		if ( bTrackCreated )
		{
			if (OnInitializeNewTrack)
			{
				OnInitializeNewTrack(Track);
			}
		}

		if ( Track )
		{
			bSectionCreated = AddKeysToTrack( Track, KeyTime, NewKeys, DefaultKeys, KeyMode, bTrackCreated );
		}

		return bTrackCreated || bSectionCreated;
	}

	/* Returns whether a section was added */
	bool AddKeysToTrack(
		TrackType* Track, float KeyTime,
		const TArray<KeyDataType>& NewKeys, const TArray<KeyDataType>& DefaultKeys,
		ESequencerKeyMode KeyMode, bool bNewTrack)
	{
		bool bSectionCreated = false;
		bool bInfiniteKeyAreas = GetSequencer()->GetInfiniteKeyAreas();

		if ( KeyMode != ESequencerKeyMode::AutoKey || GetSequencer()->GetAutoKeyMode() != EAutoKeyMode::KeyNone )
		{
			EMovieSceneKeyInterpolation InterpolationMode = GetSequencer()->GetKeyInterpolation();

			bool bKeyEvenIfUnchanged =
				KeyMode == ESequencerKeyMode::ManualKeyForced ||
				GetSequencer()->GetKeyAllEnabled();

			bool bKeyEvenIfEmpty =
				(KeyMode == ESequencerKeyMode::AutoKey && GetSequencer()->GetAutoKeyMode() == EAutoKeyMode::KeyAll) ||
				KeyMode == ESequencerKeyMode::ManualKeyForced;

			for (const KeyDataType& NewKey : NewKeys)
			{
				if ( NewKeyIsNewData( Track, KeyTime, NewKey ) || bKeyEvenIfUnchanged )
				{
					if ( HasKeys( Track, NewKey ) || bKeyEvenIfEmpty )
					{
						bSectionCreated |= AddKey( Track, KeyTime, NewKey, InterpolationMode, bInfiniteKeyAreas );
					}
				}
			}
		}

		if (GetSequencer()->GetAutoSetTrackDefaults())
		{
			for (const KeyDataType& NewKey : NewKeys)
			{
				bSectionCreated |= SetDefault(Track, KeyTime, NewKey, bInfiniteKeyAreas);
			}

			for (const KeyDataType& DefaultKey : DefaultKeys)
			{
				bSectionCreated |= SetDefault(Track, KeyTime, DefaultKey, bInfiniteKeyAreas);
			}
		}

		// If a new track was created but no keys or defaults were set, make sure a new section is created too to allow the user to edit it.
		if ( bNewTrack && bSectionCreated == false)
		{
			Track->Modify();
			UMovieSceneSection* NewSection = Track->FindOrAddSection(KeyTime, bSectionCreated);
			NewSection->SetIsInfinite(bInfiniteKeyAreas);
		}

		return bSectionCreated;
	}

	bool NewKeyIsNewData( TrackType* Track, float Time, const KeyDataType& KeyData ) const
	{
		IKeyframeSection<KeyDataType>* NearestKeyframeSection = CastChecked<SectionType>( MovieSceneHelpers::FindNearestSectionAtTime( Track->GetAllSections(), Time ), ECastCheckedType::NullAllowed );
		return NearestKeyframeSection == nullptr || NearestKeyframeSection->NewKeyIsNewData( Time, KeyData );
	}

	bool HasKeys( TrackType* Track, const KeyDataType& KeyData ) const
	{
		for ( UMovieSceneSection* Section : Track->GetAllSections() )
		{
			IKeyframeSection<KeyDataType>* KeyframeSection = CastChecked<SectionType>( Section );
			if ( KeyframeSection->HasKeys( KeyData ) )
			{
				return true;
			}
		}
		return false;
	}

	using FMovieSceneTrackEditor::AddKey;

	/* Return whether a section was added */
	bool AddKey( TrackType* Track, float Time, const KeyDataType& KeyData, EMovieSceneKeyInterpolation KeyInterpolation, bool bInfiniteKeyAreas )
	{
		bool bSectionAdded = false;
		Track->Modify();
		UMovieSceneSection* NewSection = Track->FindOrAddSection( Time, bSectionAdded );
		IKeyframeSection<KeyDataType>* KeyframeSection = CastChecked<SectionType>( NewSection );
		KeyframeSection->AddKey( Time, KeyData, KeyInterpolation );

		if (bSectionAdded)
		{
			NewSection->SetIsInfinite(bInfiniteKeyAreas);
		}
		return bSectionAdded;
	}

	/* Return whether a section was added */
	bool SetDefault( TrackType* Track, float Time, const KeyDataType& KeyData, bool bInfiniteKeyAreas )
	{
		bool bSectionAdded = false;
		Track->Modify();
		const TArray<UMovieSceneSection*>& Sections = Track->GetAllSections();
		if ( Sections.Num() > 0)
		{
			for ( UMovieSceneSection* Section : Sections )
			{
				IKeyframeSection<KeyDataType>* KeyframeSection = CastChecked<SectionType>( Section );
				KeyframeSection->SetDefault( KeyData );
			}
		}
		else
		{
			UMovieSceneSection* NewSection = Track->FindOrAddSection( Time, bSectionAdded );
			IKeyframeSection<KeyDataType>* KeyframeSection = CastChecked<SectionType>( NewSection );
			KeyframeSection->SetDefault( KeyData );

			if (bSectionAdded)
			{
				NewSection->SetIsInfinite(bInfiniteKeyAreas);
			}
		}
		return bSectionAdded;
	}
};


