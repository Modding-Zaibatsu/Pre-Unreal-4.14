// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequenceEditorSettings.generated.h"


class UMovieSceneTrack;
class ULevelSequence;


USTRUCT()
struct FLevelSequencePropertyTrackSettings
{
	GENERATED_BODY()

	/** Optional ActorComponent tag (when keying a component property). */
	UPROPERTY(config, EditAnywhere, Category=PropertyTrack)
	FString ComponentPath;

	/** Path to the keyed property within the Actor or ActorComponent. */
	UPROPERTY(config, EditAnywhere, Category=PropertyTrack)
	FString PropertyPath;
};


USTRUCT()
struct FLevelSequenceTrackSettings
{
	GENERATED_BODY()

	/** The Actor class to create movie scene tracks for. */
	UPROPERTY(config, noclear, EditAnywhere, Category=TrackSettings, meta=(MetaClass="Actor"))
	FStringClassReference MatchingActorClass;

	/** List of movie scene track classes to be added automatically. */
	UPROPERTY(config, noclear, EditAnywhere, Category=TrackSettings, meta=(MetaClass="MovieSceneTrack"))
	TArray<FStringClassReference> DefaultTracks;

	/** List of movie scene track classes not to be added automatically. */
	UPROPERTY(config, noclear, EditAnywhere, Category=TrackSettings, meta=(MetaClass="MovieSceneTrack"))
	TArray<FStringClassReference> ExcludeDefaultTracks;

	/** List of property names for which movie scene tracks will be created automatically. */
	UPROPERTY(config, EditAnywhere, Category=TrackSettings)
	TArray<FLevelSequencePropertyTrackSettings> DefaultPropertyTracks;

	/** List of property names for which movie scene tracks will not be created automatically. */
	UPROPERTY(config, EditAnywhere, Category=TrackSettings)
	TArray<FLevelSequencePropertyTrackSettings> ExcludeDefaultPropertyTracks;
};


/**
 * Level Sequence Editor settings.
 */
UCLASS(config=EditorPerProjectUserSettings)
class ULevelSequenceEditorSettings
	: public UObject
{
	GENERATED_BODY()

public:

	/** Specifies class properties for which movie scene tracks will be created automatically. */
	UPROPERTY(config, EditAnywhere, Category=Tracks)
	TArray<FLevelSequenceTrackSettings> TrackSettings;
};

/**
 * Level Sequence Master Sequence settings.
 */
UCLASS(config=EditorPerProjectUserSettings)
class ULevelSequenceMasterSequenceSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Master sequence name. */
	UPROPERTY(config, DisplayName="Name", EditAnywhere, Category=MasterSequence)
	FString MasterSequenceName;

	/** Master sequence suffix. */
	UPROPERTY(config, DisplayName="Suffix", EditAnywhere, Category=MasterSequence)
	FString MasterSequenceSuffix;

	/** Master sequence path. */
	UPROPERTY(config, DisplayName="Base Path", EditAnywhere, Category=MasterSequence, meta=(ContentDir))
	FDirectoryPath MasterSequenceBasePath;

	/** Master sequence number of shots. */
	UPROPERTY(config, DisplayName="Number of Shots", EditAnywhere, Category=MasterSequence, meta = (UIMin = "1", UIMax = "100"))
	uint32 MasterSequenceNumShots;

	/** Master sequence level sequence to duplicate when creating shots. */
	UPROPERTY(Transient, DisplayName="Sequence to Duplicate", EditAnywhere, Category=MasterSequence)
	TLazyObjectPtr<class ULevelSequence> MasterSequenceLevelSequenceToDuplicate;

	/** Array of sub sequence names, each will result in a level sequence asset in the shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MasterSequence)
	TArray<FName> SubSequenceNames;
};

