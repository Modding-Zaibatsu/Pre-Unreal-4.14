// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutomationScreenshotOptions.generated.h"

UENUM()
enum class EComparisonTolerance : uint8
{
	Zero,
	Low,
	Medium,
	High,
	Custom
};

USTRUCT()
struct FComparisonToleranceAmount
{
	GENERATED_BODY()

public:

	FComparisonToleranceAmount()
		: Red(0)
		, Green(0)
		, Blue(0)
		, Alpha(0)
		, MinBrightness(0)
		, MaxBrightness(255)
	{
	}

	FComparisonToleranceAmount(uint8 R, uint8 G, uint8 B, uint8 A, uint8 InMinBrightness, uint8 InMaxBrightness)
		: Red(R)
		, Green(G)
		, Blue(B)
		, Alpha(A)
		, MinBrightness(InMinBrightness)
		, MaxBrightness(InMaxBrightness)
	{
	}

public:
	UPROPERTY(EditAnywhere, Category="Tolerance")
	uint8 Red;
	
	UPROPERTY(EditAnywhere, Category="Tolerance")
	uint8 Green;
	
	UPROPERTY(EditAnywhere, Category="Tolerance")
	uint8 Blue;
	
	UPROPERTY(EditAnywhere, Category="Tolerance")
	uint8 Alpha;
	
	UPROPERTY(EditAnywhere, Category="Tolerance")
	uint8 MinBrightness;

	UPROPERTY(EditAnywhere, Category="Tolerance")
	uint8 MaxBrightness;
};


USTRUCT(BlueprintType)
struct FAutomationScreenshotOptions
{
	GENERATED_BODY()

public:
	FAutomationScreenshotOptions();

	/**
	 * The desired resolution of the screenshot, if none is provided, it will use the default for the
	 * platform setup in the automation settings.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screenshot")
	FVector2D Resolution;

	/**
	 * The delay before we take the screenshot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screenshot")
	float Delay;

	/**
	 * 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screenshot")
	bool bDisableNoisyRenderingFeatures;

	/**
	 * Allows you to screenshot a buffer other than the default final lit scene image.  Useful if you're
	 * trying to build a test for a specific GBuffer, that may be harder to tell if errors are introduced
	 * in it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screenshot")
	FName VisualizeBuffer;

	/**
	 * These are quick defaults for tolerance levels, we default to low, because generally there's some
	 * constant variability in every pixel's color introduced by TxAA.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Comparison")
	EComparisonTolerance Tolerance;

	/**
	 * For each channel and brightness levels you can control a region where the colors are found to be
	 * essentially the same.  Generally this is necessary as modern rendering techniques tend to introduce
	 * noise constantly to hide aliasing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Comparison")
	FComparisonToleranceAmount ToleranceAmount;

	/**
	 * After you've accounted for color tolerance changes, you now need to control for total acceptable error.
	 * Which depending on how pixels were colored on triangle edges may be a few percent of the image being
	 * outside the tolerance levels.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Comparison", meta=( ClampMin = 0, ClampMax = 1, UIMin = 0, UIMax = 1 ))
	float MaximumAllowedError;

	/**
	 * If this is true, we search neighboring pixels looking for the expected pixel as what may have happened, is
	 * that the pixel shifted a little.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Comparison")
	bool bIgnoreAntiAliasing;

	/**
	 * If this is true, all we compare is luminance of the scene.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Comparison", AdvancedDisplay)
	bool bIgnoreColors;

public:
	void SetToleranceAmounts(EComparisonTolerance InTolerance);
};