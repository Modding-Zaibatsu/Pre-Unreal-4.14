// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VREditorAutoScaler.generated.h"

/**
 * Automatically scales the user when dragging the world and pressing the touchpad
 */
UCLASS()
class UVREditorAutoScaler: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Initializes the automatic scaler */
	void Init( class UVREditorMode* InVRMode );

	/** Shuts down the automatic scaler */
	void Shutdown();

private:

	/** Actual scale to a certain world to meters */
	void Scale( const float NewScale );

	/** Called when the user presses a button on their motion controller device */
	void OnInteractorAction( class FEditorViewportClient& ViewportClient, class UViewportInteractor* Interactor,
		const struct FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled );

	/** Owning mode */
	UPROPERTY()
	UVREditorMode* VRMode;

	/** Teleport sound */
	UPROPERTY()
	class USoundCue* ScaleSound;
};