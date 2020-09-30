// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"


class IMediaPlayer;


/**
 * Interface for the AndroidMedia module.
 */
class IAndroidMediaModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a Android based media player.
	 *
	 * @return A new media player, or nullptr if a player couldn't be created.
	 */
	virtual TSharedPtr<IMediaPlayer> CreatePlayer() = 0;

public:

	/** Virtual destructor. */
	virtual ~IAndroidMediaModule() { }
};
