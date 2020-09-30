// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DsymExporterApp.h"

#include "Core.h"
#include "RequiredProgramMainCPPInclude.h"
#include "ExceptionHandling.h"

IMPLEMENT_APPLICATION( DsymExporter, "DsymExporter" )

 // Main entry point to the application
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FPlatformMisc::SetGracefulTerminationHandler();
	FPlatformMisc::SetCrashHandler(nullptr);
	
	GEngineLoop.PreInit( ArgC, ArgV );

	const int32 Result = RunDsymExporter( ArgC, ArgV );
	
	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	
	return Result;
}
