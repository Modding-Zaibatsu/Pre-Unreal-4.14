// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UMG : ModuleRules
{
	public UMG(TargetInfo Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/UMG/Private" // For PCH includes (because they don't work with relative paths, yet)
            })
		;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "ShaderCore",
				"RenderCore",
				"RHI",
			}
		);

        PublicDependencyModuleNames.AddRange(
            new string[] {
				"HTTP",
				"MovieScene",
                "MovieSceneTracks",
			}
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"ImageWrapper",
			}
        );

		if (Target.Type != TargetRules.TargetType.Server)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SlateRHIRenderer",
				}
			);

            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
				    "ImageWrapper",
				    "SlateRHIRenderer",
			    }
            );
		};
	}
}
