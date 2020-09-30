// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HardwareTargetingSettings.h"

class FHardwareTargetingSettingsDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	// End of IDetailCustomization interface
};

