// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealEd.h"
#include "ViewportWorldInteractionManager.h"
#include "IViewportInteractionModule.h"

class FViewportWorldInteractionManager;

class FViewportInteractionModule : public IViewportInteractionModule
{
public:
	
	FViewportInteractionModule();
	virtual ~FViewportInteractionModule();

	// FModuleInterface overrides
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual void PostLoadCallback() override;
	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}
	virtual FViewportWorldInteractionManager& GetWorldInteractionManager() override;
	virtual void Tick( float DeltaTime ) override;

private:

	/** Manager that owns the current ViewportWorldInteraction */
	FViewportWorldInteractionManager WorldInteractionManager;
	
};