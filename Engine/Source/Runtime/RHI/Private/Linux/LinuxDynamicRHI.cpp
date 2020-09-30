// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "RHIPrivatePCH.h"
#include "RHI.h"
#include "ModuleManager.h"

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = NULL;
	// Load the dynamic RHI module.
	IDynamicRHIModule* DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));

	if (!DynamicRHIModule->IsSupported())
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "RequiredOpenGL", "OpenGL 3.2 is required to run the engine."));
		FPlatformMisc::RequestExit(1);
		DynamicRHIModule = NULL;
	}

	// default to SM4 for safety's sake
	ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::SM4;

	// Check the list of targeted shader platforms and decide an RHI based off them
	TArray<FString> TargetedShaderFormats;
	GConfig->GetArray(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);
	if (TargetedShaderFormats.Num() > 0)
	{
		// Pick the first one
		FName ShaderFormatName(*TargetedShaderFormats[0]);
		EShaderPlatform TargetedPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
		RequestedFeatureLevel = GetMaxSupportedFeatureLevel(TargetedPlatform);
	}

	// Create the dynamic RHI.
	if (DynamicRHIModule)
	{
		DynamicRHI = DynamicRHIModule->CreateRHI(RequestedFeatureLevel);
	}

	return DynamicRHI;
}
