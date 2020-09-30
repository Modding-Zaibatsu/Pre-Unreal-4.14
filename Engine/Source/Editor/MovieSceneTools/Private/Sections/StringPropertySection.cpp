// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "StringCurveKeyArea.h"
#include "StringPropertySection.h"
#include "MovieSceneStringSection.h"


void FStringPropertySection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	UMovieSceneStringSection* StringSection = Cast<UMovieSceneStringSection>(&SectionObject);

	TAttribute<TOptional<FString>> ExternalValue;
	ExternalValue.Bind(TAttribute<TOptional<FString>>::FGetter::CreateLambda([&] { return GetPropertyValue<FString>(); }));

	TSharedRef<FStringCurveKeyArea> KeyArea = MakeShareable(new FStringCurveKeyArea(&StringSection->GetStringCurve(), ExternalValue, StringSection));
	LayoutBuilder.SetSectionAsKeyArea(KeyArea);
}
