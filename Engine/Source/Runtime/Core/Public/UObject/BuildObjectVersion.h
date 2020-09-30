// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

// Custom serialization version for changes made in Dev-Build stream
struct CORE_API FBuildObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,


		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FBuildObjectVersion() {}
};
