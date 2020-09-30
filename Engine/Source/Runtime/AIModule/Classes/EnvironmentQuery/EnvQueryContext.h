// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvQueryContext.generated.h"

struct FEnvQueryContextData;
struct FEnvQueryInstance;

UCLASS(Abstract, EditInlineNew)
class AIMODULE_API UEnvQueryContext : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const;
};
