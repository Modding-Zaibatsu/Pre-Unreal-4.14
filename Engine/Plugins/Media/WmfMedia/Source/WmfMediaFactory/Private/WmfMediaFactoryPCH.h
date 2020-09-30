// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
	#include "Developer/Settings/Public/ISettingsModule.h"
	#include "Developer/Settings/Public/ISettingsSection.h"
#endif

#include "Runtime/Core/Public/Core.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Runtime/CoreUObject/Public/CoreUObject.h"
#include "Runtime/Media/Public/IMediaModule.h"
#include "Runtime/Media/Public/IMediaOptions.h"

#include "../../WmfMedia/Public/IWmfMediaModule.h"


/** Log category for the WmfMediaFactory module. */
DECLARE_LOG_CATEGORY_EXTERN(LogWmfMediaFactory, Log, All);
