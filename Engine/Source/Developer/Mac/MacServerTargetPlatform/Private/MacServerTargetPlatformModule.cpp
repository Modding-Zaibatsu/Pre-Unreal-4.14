// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacServerTargetPlatformModule.cpp: Implements the FMacServerTargetPlatformModule class.
=============================================================================*/

#include "MacServerTargetPlatformPrivatePCH.h"


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for the Mac target platform (without editor).
 */
class FMacServerTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual ~FMacServerTargetPlatformModule( )
	{
		Singleton = NULL;
	}

	virtual ITargetPlatform* GetTargetPlatform( ) override
	{
		if (Singleton == NULL)
		{
			Singleton = new TGenericMacTargetPlatform<false, true, false>();
		}

		return Singleton;
	}
};


IMPLEMENT_MODULE(FMacServerTargetPlatformModule, MacServerTargetPlatform);