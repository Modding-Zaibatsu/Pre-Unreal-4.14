// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
// 
// Engine global defines

#pragma once

/** Global engine pointer. Can be 0 so don't use without checking. */
extern ENGINE_API class UEngine*			GEngine;

/** when set, disallows all network travel (pending level rejects all client travel attempts) */
extern ENGINE_API bool						GDisallowNetworkTravel;

/** The GPU time taken to render the last frame. Same metric as FPlatformTime::Cycles(). */
extern ENGINE_API uint32					GGPUFrameTime;

/** A reference to the ability system so we do not need to get the module every time we access the tag manager. */
extern ENGINE_API class UGameplayTagsManager* GGameplayTagsManager;