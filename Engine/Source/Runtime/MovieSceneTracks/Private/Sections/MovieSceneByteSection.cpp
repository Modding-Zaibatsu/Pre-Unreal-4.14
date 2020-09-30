// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneByteSection.h"


UMovieSceneByteSection::UMovieSceneByteSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{ }


uint8 UMovieSceneByteSection::Eval( float Position, uint8 DefaultValue) const
{
	return ByteCurve.Evaluate(Position, DefaultValue);
}


void UMovieSceneByteSection::MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection( DeltaPosition, KeyHandles );

	ByteCurve.ShiftCurve(DeltaPosition, KeyHandles);
}


void UMovieSceneByteSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
	ByteCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneByteSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(ByteCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = ByteCurve.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}


TOptional<float> UMovieSceneByteSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	if ( ByteCurve.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( ByteCurve.GetKeyTime( KeyHandle ) );
	}
	return TOptional<float>();
}


void UMovieSceneByteSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	if ( ByteCurve.IsKeyHandleValid( KeyHandle ) )
	{
		ByteCurve.SetKeyTime( KeyHandle, Time );
	}
}


void UMovieSceneByteSection::AddKey( float Time, const uint8& Value, EMovieSceneKeyInterpolation KeyInterpolation )
{
	if (TryModify())
	{
		ByteCurve.UpdateOrAddKey(Time, Value);
	}
}


bool UMovieSceneByteSection::NewKeyIsNewData( float Time, const uint8& Value ) const
{
	return Eval( Time, Value ) != Value;
}


bool UMovieSceneByteSection::HasKeys( const uint8& Value ) const
{
	return ByteCurve.GetNumKeys() > 0;
}


void UMovieSceneByteSection::SetDefault( const uint8& Value )
{
	if (TryModify())
	{
		ByteCurve.SetDefaultValue(Value);
	}
}


void UMovieSceneByteSection::ClearDefaults()
{
	ByteCurve.ClearDefaultValue();
}
