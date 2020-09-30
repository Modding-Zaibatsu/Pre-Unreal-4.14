// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

// Any object that is capable of taking commands.
class CORE_API FExec
{
public:
	virtual ~FExec();

	/**
	* Exec handler
	*
	* @param	InWorld	World context
	* @param	Cmd		Command to parse
	* @param	Ar		Output device to log to
	*
	* @return	true if command was handled, false otherwise
	*/
	virtual bool Exec( class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) PURE_VIRTUAL(FExec::Exec,return false;)
};


