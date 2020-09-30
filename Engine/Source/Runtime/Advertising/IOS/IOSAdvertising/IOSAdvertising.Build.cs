// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class IOSAdvertising : ModuleRules
	{
		public IOSAdvertising( TargetInfo Target )
		{
			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
                    // ... add other private include paths required here ...
				}
				);

			
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Advertising",
                    // ... add private dependencies that you statically link with here ...
				}
				);
			PublicIncludePathModuleNames.Add("Advertising");

            PrivateIncludePathModuleNames.AddRange(
                new string[]
                {
                    // ... add any private module dependencies that should include paths
                }
                );

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
                    // ... add any modules that your module loads dynamically here ...
				}
				);
		}
	}
}