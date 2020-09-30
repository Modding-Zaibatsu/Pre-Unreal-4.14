// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CEF3

#if PLATFORM_WINDOWS
	#include "AllowWindowsPlatformTypes.h"
#endif

#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
#include "include/cef_task.h"
#pragma pop_macro("OVERRIDE")


#if PLATFORM_WINDOWS
	#include "HideWindowsPlatformTypes.h"
#endif

// Helper for posting a closure as a task
class FCEFBrowserClosureTask
	: public CefTask
{
public:
	FCEFBrowserClosureTask(CefRefPtr<CefBase> InHandle, TFunction<void ()> InClosure)
		: Handle(InHandle)
		, Closure(InClosure)
	{ }

	virtual void Execute() override
	{
		Closure();
	}

private:
	CefRefPtr<CefBase> Handle; // Used so the handler will not go out of scope before the closure is executed.
	TFunction<void ()> Closure;
	IMPLEMENT_REFCOUNTING(FCEFBrowserClosureTask);
};


#endif /* WITH_CEF3 */
