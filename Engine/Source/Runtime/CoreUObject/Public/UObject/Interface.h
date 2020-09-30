// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectMacros.h"

/**
 * Base class for all interfaces
 *
 */

class COREUOBJECT_API UInterface : public UObject
{
	DECLARE_CLASS_INTRINSIC(UInterface, UObject, CLASS_Interface | CLASS_Abstract, TEXT("/Script/CoreUObject"))
};

class COREUOBJECT_API IInterface
{
protected:

	virtual ~IInterface() {}

public:

	typedef UInterface UClassType;
};

