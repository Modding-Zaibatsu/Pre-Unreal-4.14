﻿// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UElibPNG : ModuleRules
{
	public UElibPNG(TargetInfo Target)
	{
		Type = ModuleType.External;

		string libPNGPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string LibPath = libPNGPath + "/lib/Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(LibPath);

			string LibFileName = "libpng" + (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT ? "d" : "") + "_64.lib";
			PublicAdditionalLibraries.Add(LibFileName);
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 ||
				(Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32") // simulator
		)
		{
			libPNGPath = libPNGPath + "/lib/Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(libPNGPath);

			string LibFileName = "libpng" + (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT ? "d" : "") + ".lib";
			PublicAdditionalLibraries.Add(LibFileName);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(libPNGPath + "/lib/Mac/libpng.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			if (Target.Architecture == "-simulator")
			{
				PublicLibraryPaths.Add(libPNGPath + "/lib/ios/Simulator");
				PublicAdditionalShadowFiles.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2/lib/ios/Simulator/libpng152.a");
			}
			else
			{
				PublicLibraryPaths.Add(libPNGPath + "/lib/ios/Device");
				PublicAdditionalShadowFiles.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2/lib/ios/Device/libpng152.a");
			}

			PublicAdditionalLibraries.Add("png152");
		}
		else if (Target.Platform == UnrealTargetPlatform.TVOS)
		{
			if (Target.Architecture == "-simulator")
			{
				PublicLibraryPaths.Add(libPNGPath + "/lib/TVOS/Simulator");
				PublicAdditionalShadowFiles.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2/lib/TVOS/Simulator/libpng152.a");
			}
			else
			{
				PublicLibraryPaths.Add(libPNGPath + "/lib/TVOS/Device");
				PublicAdditionalShadowFiles.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.2/lib/TVOS/Device/libpng152.a");
			}

			PublicAdditionalLibraries.Add("png152");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			libPNGPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "libPNG/libPNG-1.5.27";

			PublicLibraryPaths.Add(libPNGPath + "/lib/Android/ARMv7");
			PublicLibraryPaths.Add(libPNGPath + "/lib/Android/ARM64");
			PublicLibraryPaths.Add(libPNGPath + "/lib/Android/x86");
			PublicLibraryPaths.Add(libPNGPath + "/lib/Android/x64");

			PublicAdditionalLibraries.Add("png");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(libPNGPath + "/lib/Linux/" + Target.Architecture + "/libpng.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			PublicLibraryPaths.Add(libPNGPath + "/lib/HTML5");
			string OpimizationSuffix = "";
			if (UEBuildConfiguration.bCompileForSize)
			{
				OpimizationSuffix = "_Oz";
			}
			else
			{
				if (Target.Configuration == UnrealTargetConfiguration.Development)
				{
					OpimizationSuffix = "_O2";
				}
				else if (Target.Configuration == UnrealTargetConfiguration.Shipping)
				{
					OpimizationSuffix = "_O3";
				}
			}
			PublicAdditionalLibraries.Add(libPNGPath + "/lib/HTML5/libpng" + OpimizationSuffix + ".bc");
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PublicLibraryPaths.Add(libPNGPath + "/lib/PS4");
			PublicAdditionalLibraries.Add("png152");
		}
        else if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            // Use reflection to allow type not to exist if console code is not present
            System.Type XboxOnePlatformType = System.Type.GetType("UnrealBuildTool.XboxOnePlatform,UnrealBuildTool");
            if (XboxOnePlatformType != null)
            {
                System.Object VersionName = XboxOnePlatformType.GetMethod("GetVisualStudioCompilerVersionName").Invoke(null, null);
                PublicLibraryPaths.Add(libPNGPath + "/lib/XboxOne/VS" + VersionName.ToString());
                PublicAdditionalLibraries.Add("libpng125_XboxOne.lib");
            }
        }
		else if (Target.Platform == UnrealTargetPlatform.WolfPlat)
		{
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(libPNGPath, "lib/WolfPlat/libPNG.a"));
		}

		PublicIncludePaths.Add(libPNGPath);
	}
}
