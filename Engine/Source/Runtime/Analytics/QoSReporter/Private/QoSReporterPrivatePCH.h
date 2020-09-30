// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// You should place include statements to your module's private header files here.  You only need to
// add includes for headers that are used in most of your module's source files though.

#pragma once

#include "Core.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQoSReporter, Log, All);

/** whether this build cares about hitches */
#define QOS_IGNORE_HITCHES					UE_EDITOR
