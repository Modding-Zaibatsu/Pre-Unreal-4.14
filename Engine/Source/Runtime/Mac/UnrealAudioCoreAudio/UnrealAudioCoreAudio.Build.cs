// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealAudioCoreAudio : ModuleRules
{
	public UnrealAudioCoreAudio(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/UnrealAudio/Private"
			}
		);

		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PublicFrameworks.AddRange(
			new string[] { 
				"CoreAudio", 
				"AudioUnit",
				"AudioToolbox"
			}
		);

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
