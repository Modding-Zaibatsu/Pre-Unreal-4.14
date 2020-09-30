// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformOutputDevices.h: Linux platform OutputDevices functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformOutputDevices.h"

struct CORE_API FLinuxOutputDevices : public FGenericPlatformOutputDevices
{
	static void							SetupOutputDevices();
	static FString						GetAbsoluteLogFilename();

	static FOutputDevice*			GetEventLog();
	static FOutputDeviceConsole*	GetLogConsole();
	static FOutputDeviceError*		GetError();
	static FFeedbackContext*		GetWarn();
};

typedef FLinuxOutputDevices FPlatformOutputDevices;
