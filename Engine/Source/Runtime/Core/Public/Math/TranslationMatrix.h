// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


class FTranslationMatrix
	: public FMatrix
{
public:

	/** Constructor translation matrix based on given vector */
	FTranslationMatrix(const FVector& Delta);

	/** Matrix factory. Return an FMatrix so we don't have type conversion issues in expressions. */
	static FMatrix Make(FVector const& Delta)
	{
		return FTranslationMatrix(Delta);
	}
};


FORCEINLINE FTranslationMatrix::FTranslationMatrix(const FVector& Delta)
	: FMatrix(
		FPlane(1.0f,	0.0f,	0.0f,	0.0f),
		FPlane(0.0f,	1.0f,	0.0f,	0.0f),
		FPlane(0.0f,	0.0f,	1.0f,	0.0f),
		FPlane(Delta.X,	Delta.Y,Delta.Z,1.0f)
	)
{ }
