// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSPlatformOutputDevicesPrivate.h: iOS implementations of output devices
=============================================================================*/

#pragma once

/**
 * Generic Output device that writes to Windows Event Log
 */
class FOutputDeviceIOSDebug :
	public FOutputDevice
{
public:
	/** Constructor, initializing member variables */
	CORE_API FOutputDeviceIOSDebug();

	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	virtual void Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category ) override;
};


class FOutputDeviceIOSError : public FOutputDeviceError
{
public:
	/** Constructor, initializing member variables */
	CORE_API FOutputDeviceIOSError();

	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	virtual void Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category ) override;

	/**
	 * Error handling function that is being called from within the system wide global
	 * error handler, e.g. using structured exception handling on the PC.
	 */
	void HandleError();

private:
	int32		ErrorPos;
};
