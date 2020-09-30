﻿// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XmlParser : ModuleRules
{
	public XmlParser( TargetInfo Target )
	{
		PublicIncludePaths.AddRange(new string[] { "Editor/XmlParser/Public" });
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"CoreUObject",
			}
			);
	}
}
