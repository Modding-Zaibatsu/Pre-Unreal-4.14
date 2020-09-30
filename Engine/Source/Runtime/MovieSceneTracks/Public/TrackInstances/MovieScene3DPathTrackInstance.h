// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene3DConstraintTrackInstance.h"

class UMovieScene3DPathTrack;
class UMovieScene3DConstraintSection;

/**
 * Instance of a UMovieScene3DPathTrack
 */
class FMovieScene3DPathTrackInstance
	: public FMovieScene3DConstraintTrackInstance
{
public:
	FMovieScene3DPathTrackInstance( UMovieScene3DPathTrack& InPathTrack );

	/** FMovieScene3DConstraintTrackInstance interface */
	virtual void UpdateConstraint(EMovieSceneUpdatePass UpdatePass, float Position, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, AActor* Actor, UMovieScene3DConstraintSection* ConstraintSection) override;
};
