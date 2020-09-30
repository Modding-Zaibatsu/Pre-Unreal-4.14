// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BuildPatchServices : ModuleRules
{
	public BuildPatchServices(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Runtime/Online/BuildPatchServices/Private",
			}
		);

		PrivateDependencyModuleNames.AddRange(
		new string[] {
				"Analytics",
				"AnalyticsET",
				"HTTP",
				"Json",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"HTTP"
			}
		);
	}
}
