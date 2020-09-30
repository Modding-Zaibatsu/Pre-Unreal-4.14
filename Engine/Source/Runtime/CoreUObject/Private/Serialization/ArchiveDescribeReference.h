// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Helper archive class to describe a reference **/
class FArchiveDescribeReference : public FArchiveUObject
{
public:
	/**
	 * Constructor
	 *
	 * @param	Src		the object to serialize which may contain a references
	 */
	FArchiveDescribeReference(UObject* Src, UObject* InTarget, FOutputDevice& InOutput);
	virtual FString GetArchiveName() const { return TEXT("FArchiveFindAllRefs"); }

	UObject*		Source;
	UObject*		Target;
	FOutputDevice&	Output;
private:
	/** Serialize a reference **/
	FArchive& operator<<( class UObject*& Obj );
};
