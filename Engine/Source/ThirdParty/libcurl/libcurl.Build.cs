// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class libcurl : ModuleRules
{
	public libcurl(TargetInfo Target)
	{
		Type = ModuleType.External;

		Definitions.Add("WITH_LIBCURL=1");
		string LibCurlPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "libcurl/curl-7.47.1/";

		// TODO: latest recompile for consoles and mobile platforms
		string OldLibCurlPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "libcurl/";

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicIncludePaths.Add(OldLibCurlPath + "include/Linux/" + Target.Architecture);
			PublicLibraryPaths.Add(OldLibCurlPath + "lib/Linux/" + Target.Architecture);

			PublicAdditionalLibraries.Add("curl");
			PublicAdditionalLibraries.Add("crypto");
			PublicAdditionalLibraries.Add("ssl");
			PublicAdditionalLibraries.Add("dl");
		}

		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// toolchain will filter properly
            PublicIncludePaths.Add(OldLibCurlPath + "include/Android/ARMv7");
            PublicLibraryPaths.Add(OldLibCurlPath + "lib/Android/ARMv7");
            PublicIncludePaths.Add(OldLibCurlPath + "include/Android/ARM64");
            PublicLibraryPaths.Add(OldLibCurlPath + "lib/Android/ARM64");
            PublicIncludePaths.Add(OldLibCurlPath + "include/Android/x86");
            PublicLibraryPaths.Add(OldLibCurlPath + "lib/Android/x86");
            PublicIncludePaths.Add(OldLibCurlPath + "include/Android/x64");
            PublicLibraryPaths.Add(OldLibCurlPath + "lib/Android/x64");

			PublicAdditionalLibraries.Add("curl");
//            PublicAdditionalLibraries.Add("crypto");
//            PublicAdditionalLibraries.Add("ssl");
//            PublicAdditionalLibraries.Add("dl");
        }
   		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string PlatformSubdir = "/Mac/";
			PublicIncludePaths.Add(LibCurlPath + "include" + PlatformSubdir);
			// OSX needs full path
			PublicAdditionalLibraries.Add(LibCurlPath + "lib" + PlatformSubdir + "libcurl.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 ||
				(Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
		{
			string PlatformSubdir = (Target.Platform == UnrealTargetPlatform.HTML5) ? "Win32" : Target.Platform.ToString();
			
			PublicIncludePaths.Add(LibCurlPath + "/include/" + PlatformSubdir +  "/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName());
			PublicLibraryPaths.Add(LibCurlPath + "/lib/" + PlatformSubdir +  "/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName());

			PublicAdditionalLibraries.Add("libcurl_a.lib");
			Definitions.Add("CURL_STATICLIB=1");
		}
	}
}
