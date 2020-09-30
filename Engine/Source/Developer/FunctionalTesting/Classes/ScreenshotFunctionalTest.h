// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FunctionalTest.h"
#include "AutomationScreenshotOptions.h"

#include "ScreenshotFunctionalTest.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, MinimalAPI)
class AScreenshotFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AScreenshotFunctionalTest(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void PrepareTest() override;
	virtual bool IsReady_Implementation() override;
	virtual void StartTest() override;

	void OnScreenshotTaken(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData);
	void SetupVisualizeBuffer();

protected:
	
	UPROPERTY()
	class UCameraComponent* ScreenshotCamera;

	UPROPERTY(EditAnywhere, Category="Screenshot", SimpleDisplay)
	FAutomationScreenshotOptions ScreenshotOptions;
};