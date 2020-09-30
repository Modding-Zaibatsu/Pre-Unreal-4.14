// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProceduralMeshComponent : ModuleRules
	{
        public ProceduralMeshComponent(TargetInfo Target)
		{
			PrivateIncludePaths.Add("ProceduralMeshComponent/Private");
            PublicIncludePaths.Add("ProceduralMeshComponent/Public");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "ShaderCore",
                    "RHI"
				}
				);
		}
	}
}
