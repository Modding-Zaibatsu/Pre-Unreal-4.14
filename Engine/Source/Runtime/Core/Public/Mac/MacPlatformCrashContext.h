// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Apple/ApplePlatformCrashContext.h"

struct CORE_API FMacCrashContext : public FApplePlatformCrashContext
{
	/** Mimics Windows WER format */
	void GenerateWindowsErrorReport(char const* WERPath, bool bIsEnsure = false) const;

	/** Copies the PLCrashReporter minidump */
	void CopyMinidump(char const* OutputPath, char const* InputPath) const;

	/** Generates the ensure/crash info into the given folder */
	void GenerateInfoInFolder(char const* const InfoFolder, bool bIsEnsure = false) const;
	
	/** Generates information for crash reporter */
	void GenerateCrashInfoAndLaunchReporter() const;
	
	/** Generates information for ensures sent via the CrashReporter */
	void GenerateEnsureInfoAndLaunchReporter() const;
};

typedef FMacCrashContext FPlatformCrashContext;
