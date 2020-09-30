// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DemoPendingNetGame.generated.h"

class UEngine;
struct FWorldContext;

UCLASS(transient, config=Engine)
class UDemoPendingNetGame
	: public UPendingNetGame
{
	GENERATED_UCLASS_BODY()

	// UPendingNetGame interface.
	virtual void Tick( float DeltaTime ) override;
	virtual void SendJoin() override;
	virtual void LoadMapCompleted(UEngine* Engine, FWorldContext& Context, bool bLoadedMapSuccessfully, const FString& LoadMapError) override;
};
