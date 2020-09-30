// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AISenseConfig.h"
#include "AISenseConfig_Touch.generated.h"

class UAISense_Touch;

UCLASS(meta = (DisplayName = "AI Touch config"))
class AIMODULE_API UAISenseConfig_Touch : public UAISenseConfig
{
	GENERATED_UCLASS_BODY()
public:	
	virtual TSubclassOf<UAISense> GetSenseImplementation() const override;
};