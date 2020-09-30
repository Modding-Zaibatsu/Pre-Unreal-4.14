// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class IPortalService
{
public:

	/** Virtual destructor. */
	virtual ~IPortalService() { }

	virtual bool IsAvailable() const = 0;
};
