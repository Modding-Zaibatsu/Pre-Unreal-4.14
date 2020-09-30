// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowsNoEditorTargetPlatformPrivatePCH.h: Pre-compiled header file for the WindowsNoEditorTargetPlatform module.
=============================================================================*/

#pragma once


/* Dependencies
 *****************************************************************************/

#include "Core.h"
#include "ModuleManager.h"
#include "WindowsPlatformProperties.h"

#if WITH_ENGINE
	#include "Engine.h"
	#include "TextureCompressorModule.h"
#endif

#include "TargetPlatform.h"
#include "TargetPlatformBase.h"

#include "AllowWindowsPlatformTypes.h"
	#include <TlHelp32.h>
#include "HideWindowsPlatformTypes.h"


/* Private includes
 *****************************************************************************/

#include "LocalPcTargetDevice.h"
#include "GenericWindowsTargetPlatform.h"
