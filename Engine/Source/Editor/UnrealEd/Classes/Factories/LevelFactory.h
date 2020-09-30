// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// LevelFactory
//~=============================================================================

#pragma once
#include "LevelFactory.generated.h"

UCLASS(MinimalAPI)
class ULevelFactory : public UFactory
{
	GENERATED_UCLASS_BODY()


	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateText( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn ) override;
	//~ Begin UFactory Interface	
};



