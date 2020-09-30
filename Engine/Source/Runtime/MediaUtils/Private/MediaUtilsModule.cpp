// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaUtilsPCH.h"


/**
 * Implements the Media module.
 */
class FMediaUtilsModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
};


IMPLEMENT_MODULE(FMediaUtilsModule, MediaUtils);
