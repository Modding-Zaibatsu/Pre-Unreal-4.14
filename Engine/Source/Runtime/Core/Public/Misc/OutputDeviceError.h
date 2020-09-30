// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OutputDevice.h"

// Error device.
class CORE_API FOutputDeviceError : public FOutputDevice
{
public:
	virtual void HandleError()=0;
};

