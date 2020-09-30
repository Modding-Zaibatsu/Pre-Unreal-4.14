// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	AndroidOutputDevices.h: Android platform OutputDevices functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformOutputDevices.h"

struct CORE_API FAndroidOutputDevices : public FGenericPlatformOutputDevices
{
	static FOutputDeviceError*	GetError();
};

typedef FAndroidOutputDevices FPlatformOutputDevices;
