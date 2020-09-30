﻿// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class PhysX : ModuleRules
{
	enum PhysXLibraryMode
	{
		Debug,
		Profile,
		Checked,
		Shipping
	}

	static PhysXLibraryMode GetPhysXLibraryMode(UnrealTargetConfiguration Config)
	{
		switch (Config)
		{
			case UnrealTargetConfiguration.Debug:
				if (BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				{
					return PhysXLibraryMode.Debug;
				}
				else
				{
					return PhysXLibraryMode.Checked;
				}
			case UnrealTargetConfiguration.Shipping:
			case UnrealTargetConfiguration.Test:
				return PhysXLibraryMode.Shipping;
			case UnrealTargetConfiguration.Development:
			case UnrealTargetConfiguration.DebugGame:
			case UnrealTargetConfiguration.Unknown:
			default:
				if (BuildConfiguration.bUseShippingPhysXLibraries)
				{
					return PhysXLibraryMode.Shipping;
				}
				else if (BuildConfiguration.bUseCheckedPhysXLibraries)
				{
					return PhysXLibraryMode.Checked;
				}
				else
				{
					return PhysXLibraryMode.Profile;
				}
		}
	}

	static string GetPhysXLibrarySuffix(PhysXLibraryMode Mode)
	{
		switch (Mode)
		{
			case PhysXLibraryMode.Debug:
				return "DEBUG";
			case PhysXLibraryMode.Checked:
				return "CHECKED";
			case PhysXLibraryMode.Profile:
				return "PROFILE";
			case PhysXLibraryMode.Shipping:
			default:
				return "";
		}
	}

	public PhysX(TargetInfo Target)
	{
		Type = ModuleType.External;

		// Determine which kind of libraries to link against
		PhysXLibraryMode LibraryMode = GetPhysXLibraryMode(Target.Configuration);
		string LibrarySuffix = GetPhysXLibrarySuffix(LibraryMode);

		Definitions.Add("WITH_PHYSX=1");
		if (UEBuildConfiguration.bCompileAPEX == false)
		{
			// Since APEX is dependent on PhysX, if APEX is not being include, set the flag properly.
			// This will properly cover the case where PhysX is compiled but APEX is not.
			Definitions.Add("WITH_APEX=0");
		}
		if (UEBuildConfiguration.bCompilePhysXVehicle == false)
		{
			// Since PhysX Vehicle is dependent on PhysX, if Vehicle is not being include, set the flag properly.
			// This will properly cover the case where PhysX is compiled but Vehicle is not.
			Definitions.Add("WITH_VEHICLE=0");
		}

		if (LibraryMode == PhysXLibraryMode.Shipping)
		{
			Definitions.Add("WITH_PHYSX_RELEASE=1");
		}
		else
		{
			Definitions.Add("WITH_PHYSX_RELEASE=0");
		}

		string PhysXVersion = "PhysX_3.4";
		string PxSharedVersion = "PxShared";

		string PhysXDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "PhysX/" + PhysXVersion + "/";
		string PxSharedDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "PhysX/" + PxSharedVersion + "/";

		string PhysXLibDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "PhysX/Lib/";
		string PxSharedLibDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "PhysX/Lib/";

		string PhysXIncludeDir = PhysXDir + "Include/";
		string PxSharedIncludeDir = PxSharedDir + "include/";
		if (Target.Platform == UnrealTargetPlatform.WolfPlat)
		{
			// all physx includes in a WolfPlat subdir
			PhysXIncludeDir = PhysXIncludeDir + "WolfPlat/";
			PxSharedIncludeDir = PxSharedIncludeDir + "WolfPlat/";
		}


		PublicSystemIncludePaths.AddRange(
			new string[] {
				PxSharedIncludeDir,
				PxSharedIncludeDir + "cudamanager",
				PxSharedIncludeDir + "filebuf",
				PxSharedIncludeDir + "foundation",
				PxSharedIncludeDir + "pvd",
				PxSharedIncludeDir + "task",
				PhysXIncludeDir,
				PhysXIncludeDir + "foundation",
				PhysXIncludeDir + "cooking",
				PhysXIncludeDir + "common",
				PhysXIncludeDir + "extensions",
				PhysXIncludeDir + "geometry",
				PhysXIncludeDir + "vehicle"
			}
			);

		// Libraries and DLLs for windows platform
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PhysXLibDir += "Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PxSharedLibDir += "Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(PhysXLibDir);
			PublicLibraryPaths.Add(PxSharedLibDir);

			string[] StaticLibrariesX64 = new string[] {
				"PhysX3{0}_x64.lib",
				"PhysX3Extensions{0}_x64.lib",
				"PhysX3Cooking{0}_x64.lib",
				"PhysX3Common{0}_x64.lib",
				"PhysX3Vehicle{0}_x64.lib",
				"PsFastXml{0}_x64.lib",
				"PxFoundation{0}_x64.lib",
				"PxPvdSDK{0}_x64.lib",
				"PxTask{0}_x64.lib",
			};

			string[] DelayLoadDLLsX64 = new string[] {
				"PxFoundation{0}_x64.dll",
				"PxPvdSDK{0}_x64.dll",
				"PhysX3{0}_x64.dll",
				"PhysX3Cooking{0}_x64.dll",
				"PhysX3Common{0}_x64.dll",
			};

			string[] PhysXRuntimeDependenciesX64 = new string[] {
				"PhysX3{0}_x64.dll",
				"PhysX3Common{0}_x64.dll",
				"PhysX3Cooking{0}_x64.dll",
			};
			string[] PxSharedRuntimeDependenciesX64 = new string[] {
				"PxFoundation{0}_x64.dll",
				"PxPvdSDK{0}_x64.dll",
			};

			foreach (string Lib in StaticLibrariesX64)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}

			foreach (string DLL in DelayLoadDLLsX64)
			{
				PublicDelayLoadDLLs.Add(String.Format(DLL, LibrarySuffix));
			}

			string PhysXBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win64/VS{0}/", WindowsPlatform.GetVisualStudioCompilerVersionName());
			foreach (string DLL in PhysXRuntimeDependenciesX64)
			{
				string FileName = PhysXBinariesDir + String.Format(DLL, LibrarySuffix);
				RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
				RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
			}

			if (LibrarySuffix != "")
			{
				Definitions.Add("UE_PHYSX_SUFFIX=" + LibrarySuffix);
			}

			string PxSharedBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win64/VS{0}/", WindowsPlatform.GetVisualStudioCompilerVersionName());
			foreach (string DLL in PxSharedRuntimeDependenciesX64)
			{
				RuntimeDependencies.Add(new RuntimeDependency(PxSharedBinariesDir + String.Format(DLL, LibrarySuffix)));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 || (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
		{
			PhysXLibDir += "Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PxSharedLibDir += "Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(PhysXLibDir);
			PublicLibraryPaths.Add(PxSharedLibDir);

			string[] StaticLibrariesX86 = new string[] {
				"PhysX3{0}_x86.lib",
				"PhysX3Extensions{0}_x86.lib",
				"PhysX3Cooking{0}_x86.lib",
				"PhysX3Common{0}_x86.lib",
				"PhysX3Vehicle{0}_x86.lib",
				"PsFastXml{0}_x86.lib",
				"PxFoundation{0}_x86.lib",
				"PxPvdSDK{0}_x86.lib",
				"PxTask{0}_x86.lib",
			};

			string[] DelayLoadDLLsX86 = new string[] {
				"PhysX3{0}_x86.dll",
				"PhysX3Cooking{0}_x86.dll",
				"PhysX3Common{0}_x86.dll"
			};

			string[] RuntimeDependenciesX86 = new string[] {
				"PhysX3{0}_x86.dll",
				"PhysX3Common{0}_x86.dll",
				"PhysX3Cooking{0}_x86.dll",
			};

			foreach (string Lib in StaticLibrariesX86)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}

			foreach (string DLL in DelayLoadDLLsX86)
			{
				PublicDelayLoadDLLs.Add(String.Format(DLL, LibrarySuffix));
			}

			string PhysXBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/PhysX/Win32/VS{0}/", WindowsPlatform.GetVisualStudioCompilerVersionName());
			foreach (string DLL in RuntimeDependenciesX86)
			{
				string FileName = PhysXBinariesDir + String.Format(DLL, LibrarySuffix);
				RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
				RuntimeDependencies.Add(Path.ChangeExtension(FileName, ".pdb"), StagedFileType.DebugNonUFS);
			}

			if (LibrarySuffix != "")
			{
				Definitions.Add("UE_PHYSX_SUFFIX=" + LibrarySuffix);
			}

		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PhysXLibDir += "Mac";
			PxSharedLibDir += "Mac";
			PublicLibraryPaths.Add(PhysXLibDir);
			PublicLibraryPaths.Add(PxSharedLibDir);

			string[] StaticLibrariesMac = new string[] {
				PhysXLibDir + "/libLowLevel{0}.a",
				PhysXLibDir + "/libLowLevelCloth{0}.a",
				PhysXLibDir + "/libPhysX3Extensions{0}.a",
				PhysXLibDir + "/libPhysX3Vehicle{0}.a",
				PhysXLibDir + "/libSceneQuery{0}.a",
				PhysXLibDir + "/libSimulationController{0}.a",
				PxSharedLibDir + "/libPxTask{0}.a",
				PxSharedLibDir + "/libPsFastXml{0}.a"
			};

			foreach (string Lib in StaticLibrariesMac)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}

			string[] DynamicLibrariesMac = new string[] {
				"/libPhysX3{0}.dylib",
				"/libPhysX3Cooking{0}.dylib",
				"/libPhysX3Common{0}.dylib",
				"/libPxFoundation{0}.dylib",
				"/libPxPvdSDK{0}.dylib",
			};

			string PhysXBinariesDir = UEBuildConfiguration.UEThirdPartyBinariesDirectory + "PhysX/Mac";
			foreach (string Lib in DynamicLibrariesMac)
			{
				string LibraryPath = PhysXBinariesDir + String.Format(Lib, LibrarySuffix);
				PublicDelayLoadDLLs.Add(LibraryPath);
				RuntimeDependencies.Add(new RuntimeDependency(LibraryPath));
			}

			if (LibrarySuffix != "")
			{
				Definitions.Add("UE_PHYSX_SUFFIX=" + LibrarySuffix);
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicLibraryPaths.Add(PhysXLibDir + "Android/ARMv7");
			PublicLibraryPaths.Add(PhysXLibDir + "Android/x86");
			PublicLibraryPaths.Add(PhysXLibDir + "Android/ARM64");
			PublicLibraryPaths.Add(PhysXLibDir + "Android/x64");

			PublicLibraryPaths.Add(PxSharedLibDir + "Android/ARMv7");
			PublicLibraryPaths.Add(PxSharedLibDir + "Android/x86");
			PublicLibraryPaths.Add(PxSharedLibDir + "Android/arm64");
			PublicLibraryPaths.Add(PxSharedLibDir + "Android/x64");

			string[] StaticLibrariesAndroid = new string[] {
				"LowLevel{0}",
				"LowLevelAABB{0}",
				"LowLevelCloth{0}",
				"LowLevelDynamics{0}",
				"LowLevelParticles{0}",
				"PhysX3{0}",
				"PhysX3Extensions{0}",
				// "PhysX3Cooking{0}", // not needed until Apex
				"PhysX3Common{0}",
				"PhysX3Vehicle{0}",
				//"PhysXVisualDebuggerSDK{0}",
				"SceneQuery{0}",
				"SimulationController{0}",
				"PxFoundation{0}",
				"PxTask{0}",
				"PxPvdSDK{0}",
				"PsFastXml{0}"
			};

			//if you are shipping, and you actually want the shipping libs, you do not need this lib
			if (!(LibraryMode == PhysXLibraryMode.Shipping && BuildConfiguration.bUseShippingPhysXLibraries))
			{
//				PublicAdditionalLibraries.Add("nvToolsExt");
			}

			foreach (string Lib in StaticLibrariesAndroid)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PhysXLibDir += "/Linux/" + Target.Architecture;
			PxSharedLibDir += "/Linux/" + Target.Architecture;

			PublicLibraryPaths.Add(PhysXLibDir);
			PublicLibraryPaths.Add(PxSharedLibDir);

			string[] StaticLibrariesLinux = new string[] {
				"rt",
				"LowLevel{0}",
				"LowLevelAABB{0}",
				"LowLevelCloth{0}",
				"LowLevelDynamics{0}",
				"LowLevelParticles{0}",
				"NvParameterized{0}",
                "PhysX3{0}",
				"PhysX3Extensions{0}",
				"PhysX3Cooking{0}",
				"PhysX3Common{0}",
				"PhysX3Vehicle{0}",
				"SceneQuery{0}",
				"SimulationController{0}",
				"PxFoundation{0}",
				"PxTask{0}",
				"PxPvdSDK{0}",
				"PsFastXml{0}",
				"RenderDebug{0}"
			};

			foreach (string Lib in StaticLibrariesLinux)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PhysXLibDir = Path.Combine(PhysXLibDir, "IOS/");
			PxSharedLibDir = Path.Combine(PxSharedLibDir, "IOS/");
			PublicLibraryPaths.Add(PhysXLibDir);
			PublicLibraryPaths.Add(PxSharedLibDir);

			string[] PhysXLibs = new string[]
				{
					"LowLevel",
					"LowLevelAABB",
					"LowLevelCloth",
					"LowLevelDynamics",
					"LowLevelParticles",
					"PhysX3",
					"PhysX3Common",
					// "PhysX3Cooking", // not needed until Apex
					"PhysX3Extensions",
					"PhysX3Vehicle",
					"SceneQuery",
					"SimulationController",
					"PxFoundation",
					"PxTask",
					"PxPvdSDK",
					"PsFastXml"
				};

			foreach (string PhysXLib in PhysXLibs)
			{
				PublicAdditionalLibraries.Add(PhysXLib + LibrarySuffix);
				PublicAdditionalShadowFiles.Add(Path.Combine(PhysXLibDir, "lib" + PhysXLib + LibrarySuffix + ".a"));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PhysXLibDir = Path.Combine(PhysXLibDir, "TVOS/");
			PxSharedLibDir = Path.Combine(PxSharedLibDir, "TVOS/");
			PublicLibraryPaths.Add(PhysXLibDir);
			PublicLibraryPaths.Add(PxSharedLibDir);

			string[] PhysXLibs = new string[]
				{
					"LowLevel",
					"LowLevelAABB",
					"LowLevelCloth",
					"LowLevelDynamics",
					"LowLevelParticles",
					"PhysX3",
					"PhysX3Common",
					// "PhysX3Cooking", // not needed until Apex
					"PhysX3Extensions",
					"PhysX3Vehicle",
					"SceneQuery",
					"SimulationController",
					"PxFoundation",
					"PxTask",
					"PxPvdSDK",
					"PsFastXml"
				};

			foreach (string PhysXLib in PhysXLibs)
			{
				PublicAdditionalLibraries.Add(PhysXLib + LibrarySuffix);
				PublicAdditionalShadowFiles.Add(Path.Combine(PhysXLibDir, "lib" + PhysXLib + LibrarySuffix + ".a"));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			PhysXLibDir = Path.Combine(PhysXLibDir, "HTML5/");
			PxSharedLibDir = Path.Combine(PxSharedLibDir, "HTML5/");

			string[] PhysXLibs = new string[]
				{
					"LowLevel",
					"LowLevelCloth",
					"PhysX3",
					"PhysX3Common",
					"PhysX3Cooking",
					"PhysX3Extensions",
					//"PhysXVisualDebuggerSDK",
					"SceneQuery",
					"SimulationController",
					"PxFoundation",
					"PxTask",
					"PxPvdSDK",
					"PsFastXml"
				};

			foreach (var lib in PhysXLibs)
			{
				if (!lib.Contains("Cooking") || Target.IsCooked == false)
				{
					PublicAdditionalLibraries.Add(PhysXLibDir + lib + (UEBuildConfiguration.bCompileForSize ? "_Oz" : "") + ".bc");
				}
			}

			if (UEBuildConfiguration.bCompilePhysXVehicle)
			{
				string[] PhysXVehicleLibs = new string[]
				{
					"PhysX3Vehicle",
				};

				foreach (var lib in PhysXVehicleLibs)
				{
					if (!lib.Contains("Cooking") || Target.IsCooked == false)
					{
						PublicAdditionalLibraries.Add(PhysXLibDir + lib + (UEBuildConfiguration.bCompileForSize ? "_Oz" : "") + ".bc");
					}
				}
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PublicLibraryPaths.Add(PhysXLibDir + "PS4");

			string[] StaticLibrariesPS4 = new string[] {
				"PhysX3{0}",
				"PhysX3Extensions{0}",
				"PhysX3Cooking{0}",
				"PhysX3Common{0}",
				"PhysX3Vehicle{0}",
				"LowLevel{0}",
				"LowLevelAABB{0}",
				"LowLevelCloth{0}",
				"LowLevelDynamics{0}",
				"LowLevelParticles{0}",
				"SceneQuery{0}",
				"SimulationController{0}",
				"PxFoundation{0}",
				"PxTask{0}",
				"PxPvdSDK{0}",
				"PsFastXml{0}"
			};

			foreach (string Lib in StaticLibrariesPS4)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			Definitions.Add("PX_PHYSX_STATIC_LIB=1");
			Definitions.Add("_XBOX_ONE=1");

			PublicLibraryPaths.Add(Path.Combine(PhysXLibDir,"XboxOne\\VS" + WindowsPlatform.GetVisualStudioCompilerVersionName()));

			string[] StaticLibrariesXB1 = new string[] {
				"PhysX3{0}.lib",
				"PhysX3Extensions{0}.lib",
				"PhysX3Cooking{0}.lib",
				"PhysX3Common{0}.lib",
				"PhysX3Vehicle{0}.lib",
				"LowLevel{0}.lib",
				"LowLevelAABB{0}.lib",
				"LowLevelCloth{0}.lib",
				"LowLevelDynamics{0}.lib",
				"LowLevelParticles{0}.lib",
				"SceneQuery{0}.lib",
				"SimulationController{0}.lib",
				"PxFoundation{0}.lib",
				"PxTask{0}.lib",
				"PxPvdSDK{0}.lib",
				"PsFastXml{0}.lib"
			};

			foreach (string Lib in StaticLibrariesXB1)
			{
				PublicAdditionalLibraries.Add(String.Format(Lib, LibrarySuffix));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.WolfPlat)
		{
			PublicLibraryPaths.Add(PhysXLibDir + "WolfPlat");
			PublicLibraryPaths.Add(PxSharedLibDir + "WolfPlat");

			string[] StaticLibrariesWolf = new string[] {
					"LowLevel",
					"LowLevelAABB",
					"LowLevelCloth",
					"LowLevelDynamics",
					"LowLevelParticles",
					"PhysX3",
					"PhysX3Common",
					// "PhysX3Cooking", // not needed until Apex
					"PhysX3Extensions",
					"PhysX3Vehicle",
					"SceneQuery",
					"SimulationController",
					"PxFoundation",
					"PxTask",
					"PxPvdSDK",
					"PsFastXml"
			};

			foreach (string Lib in StaticLibrariesWolf)
			{
				PublicAdditionalLibraries.Add(Lib + LibrarySuffix);
			}
		}
	}
}
