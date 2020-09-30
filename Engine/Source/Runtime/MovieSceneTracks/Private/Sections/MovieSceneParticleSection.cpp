// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneParticleSection.h"


UMovieSceneParticleSection::UMovieSceneParticleSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	ParticleKeys.SetDefaultValue((int32)EParticleKey::Deactivate);
	ParticleKeys.SetUseDefaultValueBeforeFirstKey(true);
	SetIsInfinite(true);
}

void UMovieSceneParticleSection::AddKey( float Time, EParticleKey::Type KeyType )
{
	ParticleKeys.AddKey( Time, (int32)KeyType );
}

FIntegralCurve& UMovieSceneParticleSection::GetParticleCurve()
{
	return ParticleKeys;
}

void UMovieSceneParticleSection::MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	ParticleKeys.ShiftCurve( DeltaPosition, KeyHandles );
}

void UMovieSceneParticleSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Super::DilateSection( DilationFactor, Origin, KeyHandles );

	ParticleKeys.ScaleCurve( Origin, DilationFactor, KeyHandles );
}

void UMovieSceneParticleSection::GetKeyHandles( TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange ) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for ( auto It( ParticleKeys.GetKeyHandleIterator() ); It; ++It )
	{
		float Time = ParticleKeys.GetKeyTime( It.Key() );
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add( It.Key() );
		}
	}
}


TOptional<float> UMovieSceneParticleSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	if ( ParticleKeys.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( ParticleKeys.GetKeyTime( KeyHandle ) );
	}
	return TOptional<float>();
}


void UMovieSceneParticleSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	if ( ParticleKeys.IsKeyHandleValid( KeyHandle ) )
	{
		ParticleKeys.SetKeyTime( KeyHandle, Time );
	}
}
