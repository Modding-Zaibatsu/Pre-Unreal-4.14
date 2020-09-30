// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 *	RGBA Color made up of FFloat16
 */
class FFloat16Color
{
public:

	FFloat16 R;
	FFloat16 G;
	FFloat16 B;
	FFloat16 A;

	/** Default constructor */
	FFloat16Color();

	/** Copy constructor. */
	FFloat16Color(const FFloat16Color& Src);

	/** Constructor from a linear color. */
	FFloat16Color(const FLinearColor& Src);

	/** assignment operator */
	FFloat16Color& operator=(const FFloat16Color& Src);

 	/**
	 * Checks whether two colors are identical.
	 *
	 * @param Src The other color.
	 * @return true if the two colors are identical, otherwise false.
	 */
	bool operator==(const FFloat16Color& Src);
};


FORCEINLINE FFloat16Color::FFloat16Color() { }


FORCEINLINE FFloat16Color::FFloat16Color(const FFloat16Color& Src)
{
	R = Src.R;
	G = Src.G;
	B = Src.B;
	A = Src.A;
}


FORCEINLINE FFloat16Color::FFloat16Color(const FLinearColor& Src) :
	R(Src.R),
	G(Src.G),
	B(Src.B),
	A(Src.A)
{ }


FORCEINLINE FFloat16Color& FFloat16Color::operator=(const FFloat16Color& Src)
{
	R = Src.R;
	G = Src.G;
	B = Src.B;
	A = Src.A;
	return *this;
}


FORCEINLINE bool FFloat16Color::operator==(const FFloat16Color& Src)
{
	return (
		(R == Src.R) &&
		(G == Src.G) &&
		(B == Src.B) &&
		(A == Src.A)
		);
}
