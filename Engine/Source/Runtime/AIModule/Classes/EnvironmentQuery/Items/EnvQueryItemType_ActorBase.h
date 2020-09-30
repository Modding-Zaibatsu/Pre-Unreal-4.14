// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvQueryItemType_ActorBase.generated.h"

struct FBlackboardKeySelector;
class UBlackboardComponent;

UCLASS(Abstract)
class AIMODULE_API UEnvQueryItemType_ActorBase : public UEnvQueryItemType_VectorBase
{
	GENERATED_BODY()

public:
	virtual AActor* GetActor(const uint8* RawData) const;

	virtual void AddBlackboardFilters(FBlackboardKeySelector& KeySelector, UObject* FilterOwner) const override;
	virtual bool StoreInBlackboard(FBlackboardKeySelector& KeySelector, UBlackboardComponent* Blackboard, const uint8* RawData) const override;
	virtual FString GetDescription(const uint8* RawData) const override;
};
