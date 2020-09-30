// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BufferArchive.h"

/**
 * Archive objects that are also a TArray. Since FBufferArchive is already the 
 * writer version, we just typedef to the old, outdated name
 */
typedef FBufferArchive FArrayWriter;
