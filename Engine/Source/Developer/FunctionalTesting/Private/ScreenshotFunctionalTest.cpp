// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"

#include "ScreenshotFunctionalTest.h"
#include "AutomationBlueprintFunctionLibrary.h"
#include "BufferVisualizationData.h"
#include "AutomationScreenshotOptions.h"

AScreenshotFunctionalTest::AScreenshotFunctionalTest( const FObjectInitializer& ObjectInitializer )
	: AFunctionalTest(ObjectInitializer)
{
	ScreenshotCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	ScreenshotCamera->SetupAttachment(RootComponent);
}

void AScreenshotFunctionalTest::PrepareTest()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	PlayerController->SetViewTarget(this, FViewTargetTransitionParams());

	SetupVisualizeBuffer();

	Super::PrepareTest();
}

bool AScreenshotFunctionalTest::IsReady_Implementation()
{
	return (GFrameNumber - RunFrame) > 2;
}

void AScreenshotFunctionalTest::StartTest()
{
	Super::StartTest();

	UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotInternal(GetName(), ScreenshotOptions);

	GEngine->GameViewport->OnScreenshotCaptured().AddUObject(this, &AScreenshotFunctionalTest::OnScreenshotTaken);

	//TSharedRef<IAutomationLatentCommand> CommandPtr = MakeShareable(NewCommand);
	//FAutomationTestFramework::Get().EnqueueLatentCommand(CommandPtr);
}

void AScreenshotFunctionalTest::OnScreenshotTaken(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData)
{
	GEngine->GameViewport->OnScreenshotCaptured().RemoveAll(this);

	FinishTest(EFunctionalTestResult::Succeeded, TEXT(""));
}

void AScreenshotFunctionalTest::SetupVisualizeBuffer()
{
	UWorld* World = GetWorld();
	if ( World && World->IsGameWorld() )
	{
		if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
		{
			static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
			if ( ICVar )
			{
				if ( ViewportClient->GetEngineShowFlags() )
				{
					ViewportClient->GetEngineShowFlags()->SetVisualizeBuffer( ScreenshotOptions.VisualizeBuffer == NAME_None ? false : true);
					ICVar->Set(*ScreenshotOptions.VisualizeBuffer.ToString());
				}
			}
		}
	}
}

#if WITH_EDITOR

bool AScreenshotFunctionalTest::CanEditChange(const UProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);
	if ( bIsEditable && InProperty )
	{
		const FName PropertyName = InProperty->GetFName();

		if ( PropertyName == GET_MEMBER_NAME_CHECKED(FAutomationScreenshotOptions, ToleranceAmount) )
		{
			bIsEditable = ScreenshotOptions.Tolerance == EComparisonTolerance::Custom;
		}
		else if ( PropertyName == TEXT("ObservationPoint") )
		{
			// You can't ever observe from anywhere but the camera on the screenshot test.
			bIsEditable = false;
		}
	}

	return bIsEditable;
}

void AScreenshotFunctionalTest::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if ( PropertyChangedEvent.Property )
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();

		if ( PropertyName == GET_MEMBER_NAME_CHECKED(FAutomationScreenshotOptions, Tolerance) )
		{
			ScreenshotOptions.SetToleranceAmounts(ScreenshotOptions.Tolerance);
		}
	}
}

#endif