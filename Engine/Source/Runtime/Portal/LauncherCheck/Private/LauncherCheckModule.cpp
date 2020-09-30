// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LauncherCheckPrivatePCH.h"

#if WITH_LAUNCHERCHECK

#include "GenericPlatformHttp.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"

/**
 * Log categories for LauncherCheck module
 */
DEFINE_LOG_CATEGORY(LogLauncherCheck);

/**
 * Implements the Launcher Check module.
 */
class FLauncherCheckModule
	: public ILauncherCheckModule
{
public:

	/*
	* Check to see if this module should perform any checks or not
	* @return true, if it should
	*/
	bool IsEnabled() const
	{
		return FParse::Param(FCommandLine::Get(), TEXT("NoEpicPortal")) == false;
	}

	// ILauncherCheckModule interface

	virtual bool WasRanFromLauncher() const override
	{
		// Check for the presence of a specific param that's passed from the Launcher to the games
		// when we want to make sure we've come from the Launcher
		return !IsEnabled() || FParse::Param(FCommandLine::Get(), TEXT("EpicPortal"));
	}

	virtual bool RunLauncher(ELauncherAction Action) const override
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (DesktopPlatform != nullptr)
		{
			// Construct a url to tell the launcher of this app and what we want to do with it
			FOpenLauncherOptions LauncherOptions;
			LauncherOptions.LauncherRelativeUrl = TEXT("apps");
			LauncherOptions.LauncherRelativeUrl /= GetEncodedExePath();
			switch (Action)
			{
			case ELauncherAction::AppLaunch:
				LauncherOptions.LauncherRelativeUrl += TEXT("?action=launch");
				break;
			case ELauncherAction::AppUpdateCheck:
				LauncherOptions.LauncherRelativeUrl += TEXT("?action=updatecheck");
				break;
			};
			return DesktopPlatform->OpenLauncher(LauncherOptions);
		}
		return false;
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

private:

	/**
	 * Return url encoded full path of currently running exe
	 */
	FString GetEncodedExePath() const
	{
		FString EncodedExePath;

		static const TCHAR* Delims[] = { TEXT("/") };
		static const int32 NumDelims = ARRAY_COUNT(Delims);

		// Get the path to the executable, and encode it ('cos spaces, colons, etc break things)
		FString ExePath = FPlatformProcess::BaseDir();

		// Make sure it's not relative and the slashes are the right way
		ExePath = FPaths::ConvertRelativePathToFull(ExePath);
		ExePath.ReplaceInline(TEXT("\\"), TEXT("/"));

		// Encode the path 'cos symbols like ':',' ' etc are bad
		TArray<FString> ExeFolders;
		ExePath.ParseIntoArray(ExeFolders, Delims, NumDelims);
		for (const FString& ExeFolder : ExeFolders)
		{
			EncodedExePath /= FGenericPlatformHttp::UrlEncode(ExeFolder);
		}
		// Make sure it ends in a slash
		EncodedExePath /= TEXT("");

		return EncodedExePath;
	}
};


#else

class FLauncherCheckModule
	: public ILauncherCheckModule
{
public:

	virtual bool WasRanFromLauncher() const override { return true; }

	virtual bool RunLauncher(ELauncherAction Action) const override { return false; }

};


#endif // WITH_LAUNCHERCHECK

IMPLEMENT_MODULE(FLauncherCheckModule, LauncherCheck );


