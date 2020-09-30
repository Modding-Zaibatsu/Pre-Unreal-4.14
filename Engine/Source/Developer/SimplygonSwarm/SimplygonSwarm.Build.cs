// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SimplygonSwarm : ModuleRules
{
	public SimplygonSwarm(TargetInfo Target)
	{
		PublicIncludePaths.Add("Developer/SimplygonSwarm/Public");
        PrivateIncludePaths.Add("Developer/SimplygonSwarm/Private");

        PublicDependencyModuleNames.AddRange(
        new string[] { 
				"Core",
				"CoreUObject",
				"InputCore",
				"Json",
				"RHI"
			}
        );

        PrivateDependencyModuleNames.AddRange(
        new string[] { 
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"RawMesh",
                "MeshBoneReduction",
                "ImageWrapper",
                "HTTP",
                "Json",
                "UnrealEd",
                "MaterialUtilities"
			}
        );

        PrivateIncludePathModuleNames.AddRange(
        new string[] { 
				"MeshUtilities",
				"MaterialUtilities",
                "SimplygonMeshReduction"
			}
        );

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Simplygon");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "SSF");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "SPL");
		AddEngineThirdPartyPrivateDynamicDependencies(Target, "PropertyEditor");

		if(Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrecompileForTargets = PrecompileTargetsType.Editor;
		}
		else
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
