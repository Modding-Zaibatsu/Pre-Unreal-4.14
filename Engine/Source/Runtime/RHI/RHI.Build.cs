// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class RHI : ModuleRules
{
	public RHI(TargetInfo Target)
	{
		PrivateDependencyModuleNames.Add("Core");

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
            DynamicallyLoadedModuleNames.Add("NullDrv");

			// UEBuildAndroid.cs adds VulkanRHI for Android builds if it is enabled
			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
			{
				DynamicallyLoadedModuleNames.Add("D3D11RHI");

				//#todo-rco: D3D12 requires different SDK headers not compatible with WinXP
				DynamicallyLoadedModuleNames.Add("D3D12RHI");
            }

			if ((Target.Platform == UnrealTargetPlatform.Win64) ||
				(Target.Platform == UnrealTargetPlatform.Win32))
			{
				DynamicallyLoadedModuleNames.Add("VulkanRHI");
			}

			// need to dynamically load an assembly to check if the SDK exists (not everyone will have access to this)
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				System.Type WolfSDKType = System.Type.GetType("UnrealBuildTool.WolfPlatformSDK,UnrealBuildTool");
				if (WolfSDKType != null)
				{
					// check for the location of the SDK
					string RHIModuleName = WolfSDKType.GetMethod("GetRHIModuleNameIfAvailable").Invoke(null, null) as string;
					// compile the Wolf RHI if possible
					if (!string.IsNullOrEmpty(RHIModuleName))
					{
						DynamicallyLoadedModuleNames.Add(RHIModuleName);
						Definitions.Add("WOLFRHI=" + RHIModuleName);
					}
				}
			}

			if ((Target.Platform == UnrealTargetPlatform.Win32) ||
				(Target.Platform == UnrealTargetPlatform.Win64) ||
				(Target.Platform == UnrealTargetPlatform.Mac)   ||
                (Target.Platform == UnrealTargetPlatform.Linux && Target.Type != TargetRules.TargetType.Server) ||  // @todo should servers on all platforms skip this?
                (Target.Platform == UnrealTargetPlatform.HTML5))
			{
				DynamicallyLoadedModuleNames.Add("OpenGLDrv");
			}
        }

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

		PrivateIncludePaths.Add("Runtime/RHI/Private");
	}
}
