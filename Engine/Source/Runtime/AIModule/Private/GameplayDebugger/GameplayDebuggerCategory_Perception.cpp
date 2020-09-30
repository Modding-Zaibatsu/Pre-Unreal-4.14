// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "GameplayDebuggerCategory_Perception.h"

#if WITH_GAMEPLAY_DEBUGGER

FGameplayDebuggerCategory_Perception::FGameplayDebuggerCategory_Perception()
{
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Perception::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_Perception());
}

void FGameplayDebuggerCategory_Perception::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	APawn* MyPawn = Cast<APawn>(DebugActor);
	if (MyPawn)
	{
		AAIController* BTAI = Cast<AAIController>(MyPawn->GetController());
		if (BTAI)
		{
			UAIPerceptionComponent* PerceptionComponent = BTAI->GetPerceptionComponent();
			if (PerceptionComponent == nullptr)
			{
				PerceptionComponent = MyPawn->FindComponentByClass<UAIPerceptionComponent>();
			}

			if (PerceptionComponent)
			{
				PerceptionComponent->DescribeSelfToGameplayDebugger(this);
			}
		}
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
