// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "TickableObjectRenderThread.h"

class FSteamSplashTicker : public FTickableObjectRenderThread, public TSharedFromThis<FSteamSplashTicker>
{
public:
	FSteamSplashTicker(class FSteamVRHMD* InSteamVRHMD);

	// Registration functions for map load calls
	void RegisterForMapLoad();
	void UnregisterForMapLoad();

	// Map load delegates
	void OnPreLoadMap(const FString&);
	void OnPostLoadMap();

	// FTickableObjectRenderThread overrides
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override ;
	virtual bool IsTickable() const override;

private:
	class FSteamVRHMD* SteamVRHMD;
};