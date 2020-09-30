// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OutputDevice.h"

// Null output device.
class CORE_API FOutputDeviceNull : public FOutputDevice
{
public:
	/**
	* NULL implementation of Serialize.
	*
	* @param	Data	unused
	* @param	Event	unused
	*/
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override;
};

