// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Types of easing functions for Slate animation curves.  These are used to smooth out animations.
 */
namespace ECurveEaseFunction
{
	enum Type
	{
		/** Linear interpolation, with no easing */
		Linear,

		/** Quadratic ease in */
		QuadIn,
		
		/** Quadratic ease out */
		QuadOut,

		/** Quadratic ease in, quadratic ease out */
		QuadInOut,

		/** Cubic ease in */
		CubicIn,
		
		/** Cubic ease out */
		CubicOut,

		/** Cubic ease in, cubic ease out */
		CubicInOut,
	};
}


/**
 * A handle to curve within a curve sequence.
 */
struct SLATECORE_API FCurveHandle
{
	/**
	 * Creates and initializes a curve handle.
	 *
	 * @param InOwnerSequence The curve sequence that owns this handle.
	 * @param InCurveIndex The index of this handle.
	 */
	FCurveHandle( const struct FCurveSequence* InOwnerSequence = nullptr, int32 InCurveIndex = 0 );

public:

	/**
	 * Gets the linearly interpolated value between 0 and 1 for this curve.
	 *
	 * @return Lerp value.
	 * @see GetLerpLooping
	 */
	float GetLerp( ) const;

	/**
	 * Just like GetLerp, but loops forever.
	 *
	 * @return Looped lerp value.
	 * @see GetLerp
	 */
	DEPRECATED(4.8, "FCurveHandle::GetLerpLooping() is deprecated. Use GetLerp() instead. To play a sequence on a loop, pass \"true\" as the second parameter to FCurveSequence::Play / ::PlayReverse.")
	float GetLerpLooping( ) const;
	/** Shell to avoid generating internal deprecated warnings. Do not call. Will be removed when GetLerpLooping() is removed. */
	float DEPRECATED_GetLerpLooping() const;

	/**
	 * Checks whether this handle is initialized.
	 *
	 * A curve handle is considered initialized if it has an owner sequence.
	 * @return true if initialized, false otherwise.
	 */
	bool IsInitialized( ) const
	{
		return (OwnerSequence != nullptr);
	}

public:

	/** Applies animation easing to lerp value */
	static float ApplyEasing( float Time, ECurveEaseFunction::Type EaseType );

private:

	/** The sequence containing this curve */
	const struct FCurveSequence* OwnerSequence;

	/** The index of the curve in the Curves array */
	int32 CurveIndex;
};
