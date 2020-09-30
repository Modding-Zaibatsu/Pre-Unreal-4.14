// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneByteTrack.h"
#include "MovieSceneByteSection.h"
#include "BytePropertySection.h"
#include "EnumKeyArea.h"


void FBytePropertySection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	UMovieSceneByteSection* ByteSection = Cast<UMovieSceneByteSection>( &SectionObject );
	TAttribute<TOptional<uint8>> ExternalValue;
	ExternalValue.Bind(TAttribute<TOptional<uint8>>::FGetter::CreateLambda([&]
	{
		return GetPropertyValue<uint8>();
	}));

	if ( Enum != nullptr )
	{
		TSharedRef<FEnumKeyArea> KeyArea = MakeShareable( new FEnumKeyArea( ByteSection->GetCurve(), ExternalValue, ByteSection, Enum ) );
		LayoutBuilder.SetSectionAsKeyArea( KeyArea );
	}
	else
	{
		TSharedRef<FByteKeyArea> KeyArea = MakeShareable( new FByteKeyArea( ByteSection->GetCurve(), ExternalValue, ByteSection ) );
		LayoutBuilder.SetSectionAsKeyArea( KeyArea );
	}
}