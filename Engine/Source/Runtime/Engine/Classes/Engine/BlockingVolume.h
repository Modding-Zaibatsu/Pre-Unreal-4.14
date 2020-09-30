// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Volume.h"
#include "BlockingVolume.generated.h"


/** An invisible volume used to block other actors. */
UCLASS(MinimalAPI)
class ABlockingVolume
	: public AVolume
{
	GENERATED_UCLASS_BODY()

public:

	// UObject interface.

#if WITH_EDITOR
	virtual void LoadedFromAnotherClass(const FName& OldClassName) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR	
};
