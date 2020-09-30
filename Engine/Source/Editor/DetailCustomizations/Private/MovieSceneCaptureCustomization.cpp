// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "MovieSceneCaptureCustomization.h"
#include "MovieSceneCapture.h"

TSharedRef<IDetailCustomization> FMovieSceneCaptureCustomization::MakeInstance()
{
	return MakeShareable(new FMovieSceneCaptureCustomization);
}

void FMovieSceneCaptureCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailLayoutBuilder* CachedBuilder = &DetailBuilder;

	auto UpdateDetails = [=]{
		CachedBuilder->ForceRefreshDetails();
	};

	TSharedRef<IPropertyHandle> CaptureTypeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieSceneCapture, CaptureType));
	CaptureTypeProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda(UpdateDetails));
	CaptureTypeProperty->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda(UpdateDetails));

	TSharedRef<IPropertyHandle> ProtocolSettingsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieSceneCapture, ProtocolSettings));
	DetailBuilder.HideProperty(ProtocolSettingsProperty);

	DetailBuilder.EditCategory("CaptureSettings", FText(), ECategoryPriority::Important);

	UObject* ObjectValue = nullptr;
	ProtocolSettingsProperty->GetValue(ObjectValue);
	if (ObjectValue)
	{
		IDetailCategoryBuilder& CustomSettingsCategory = DetailBuilder.EditCategory("CustomSettings", ObjectValue->GetClass()->GetDisplayNameText(), ECategoryPriority::TypeSpecific);

		TArray<UObject*> Objects;
		Objects.Add(ObjectValue);

		for (TFieldIterator<UProperty> Iterator(ObjectValue->GetClass()); Iterator; ++Iterator)
		{
			EPropertyLocation::Type Location = Iterator->HasAnyPropertyFlags(CPF_AdvancedDisplay) ? EPropertyLocation::Advanced : EPropertyLocation::Default;
			CustomSettingsCategory.AddExternalProperty(Objects, Iterator->GetFName(), Location);
		}
	}
}
