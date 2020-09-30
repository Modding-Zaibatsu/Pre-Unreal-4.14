// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AppleMoviePlayer : ModuleRules
	{
        public AppleMoviePlayer(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
				    "CoreUObject",
				    "Engine",
                    "MoviePlayer",
                    "RenderCore",
                    "RHI",
                    "Slate"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SlateCore",
				}
			);

			PrivateIncludePaths.Add("Runtime/AppleMoviePlayer/Private");

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicFrameworks.AddRange(
					new string[] {
						"QuartzCore"
					}
				);
			}
		}
	}
}
