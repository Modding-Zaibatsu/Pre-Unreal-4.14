// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "ILocalizationServiceProvider.h"

class ULocalizationDashboardSettings;
class IPropertyHandle;
class IDetailCategoryBuilder;

struct FLocalizationServiceProviderWrapper
{
	FLocalizationServiceProviderWrapper() : Provider(nullptr) {}
	FLocalizationServiceProviderWrapper(ILocalizationServiceProvider* const InProvider) : Provider(InProvider) {}

	ILocalizationServiceProvider* Provider;
};

class FLocalizationDashboardSettingsDetailCustomization : public IDetailCustomization
{
public:
	FLocalizationDashboardSettingsDetailCustomization();
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FText GetCurrentServiceProviderDisplayName() const;
	TSharedRef<SWidget> ServiceProviderComboBox_OnGenerateWidget(TSharedPtr<FLocalizationServiceProviderWrapper> LSPWrapper) const;
	void ServiceProviderComboBox_OnSelectionChanged(TSharedPtr<FLocalizationServiceProviderWrapper> LSPWrapper, ESelectInfo::Type SelectInfo);

private:
	IDetailLayoutBuilder* DetailLayoutBuilder;

	IDetailCategoryBuilder* ServiceProviderCategoryBuilder;
	TArray< TSharedPtr<FLocalizationServiceProviderWrapper> > Providers;
};
