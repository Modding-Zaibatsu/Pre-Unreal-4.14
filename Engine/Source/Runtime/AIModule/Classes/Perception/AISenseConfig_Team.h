// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AISenseConfig.h"
#include "Perception/AISense_Team.h"
#include "AISenseConfig_Team.generated.h"

class UAISense_Team;

UCLASS(meta = (DisplayName = "AI Team sense config"))
class AIMODULE_API UAISenseConfig_Team : public UAISenseConfig
{
	GENERATED_UCLASS_BODY()
public:	
	virtual TSubclassOf<UAISense> GetSenseImplementation() const override { return UAISense_Team::StaticClass(); }
};