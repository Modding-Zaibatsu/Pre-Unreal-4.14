// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AsyncFileHandle.h"
#include "TextPackageNamespaceUtil.h"
struct FObjectInstancingGraph;

/**
 * Archive for counting memory usage.
 */
class FArchiveCountMem : public FArchiveUObject
{
public:
	FArchiveCountMem( UObject* Src )
	:	Num(0)
	,	Max(0)
	{
		ArIsCountingMemory = true;
		if( Src )
		{
			Src->Serialize( *this );
		}
	}
	SIZE_T GetNum()
	{
		return Num;
	}
	SIZE_T GetMax()
	{
		return Max;
	}
	void CountBytes( SIZE_T InNum, SIZE_T InMax )
	{
		Num += InNum;
		Max += InMax;
	}
	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const { return TEXT("FArchiveCountMem"); }

protected:
	SIZE_T Num, Max;
};
