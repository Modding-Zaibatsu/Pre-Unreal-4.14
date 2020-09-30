// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


/*=============================================================================
	Android_ASTCTargetPlatformPrivatePCH.h: Pre-compiled header file for the Android_ASTCTargetPlatform module.
=============================================================================*/

#pragma once

/* Dependencies
 *****************************************************************************/

#include "Core.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "Runtime/Core/Public/Android/AndroidProperties.h"

#if WITH_ENGINE
	#include "Engine.h"
	#include "TextureCompressorModule.h"
#endif

#include "TargetPlatform.h"
#include "TargetPlatformBase.h"
#include "AndroidDeviceDetection.h"


/* Private includes
 *****************************************************************************/

#include "AndroidTargetDevice.h"
#include "AndroidTargetPlatform.h"
