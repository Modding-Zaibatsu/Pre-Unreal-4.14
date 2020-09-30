// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TP_FirstPerson.h"
#include "TP_FirstPersonGameMode.h"
#include "TP_FirstPersonHUD.h"
#include "TP_FirstPersonCharacter.h"

ATP_FirstPersonGameMode::ATP_FirstPersonGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ATP_FirstPersonHUD::StaticClass();
}
