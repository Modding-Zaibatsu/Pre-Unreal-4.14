// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FMatineeViewSaveData
{
	FMatineeViewSaveData()
		: ViewIndex( INDEX_NONE )
		, ViewLocation( 0.0f, 0.0f, 0.0f )
		, ViewRotation( 0, 0, 0)
	{
	}

	int32 ViewIndex;
	FVector ViewLocation;
	FRotator ViewRotation;
};