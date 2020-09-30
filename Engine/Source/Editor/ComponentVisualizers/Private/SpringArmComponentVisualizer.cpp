// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizersPrivatePCH.h"
#include "SpringArmComponentVisualizer.h"
#include "GameFramework/SpringArmComponent.h"

static const FColor	ArmColor(255,0,0);

void FSpringArmComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (const USpringArmComponent* SpringArm = Cast<const USpringArmComponent>(Component))
	{
		PDI->DrawLine( SpringArm->GetComponentLocation(), SpringArm->GetSocketTransform(TEXT("SpringEndpoint"),RTS_World).GetTranslation(), ArmColor, SDPG_World );
	}
}
