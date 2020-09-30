// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetworkSettings.generated.h"


/**
 * Network settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Network"))
class ENGINE_API UNetworkSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, EditAnywhere, Category=libcurl, meta=(
		ConsoleVariable="n.VerifyPeer",DisplayName="Verify Peer",
		ToolTip="If true, libcurl authenticates the peer's certificate. Disable to allow self-signed certificates."))
	uint32 bVerifyPeer:1;

	UPROPERTY(config, EditAnywhere, Category=World, meta = (
		ConsoleVariable = "p.EnableMultiplayerWorldOriginRebasing", DisplayName = "Enable Multiplayer World Origin Rebasing",
		ToolTip="If true, origin rebasing is enabled in multiplayer games, meaning that servers and clients can have different local world origins."))
	uint32 bEnableMultiplayerWorldOriginRebasing : 1;

public:

	//~ Begin UObject Interface

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//~ End UObject Interface

};
