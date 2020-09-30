// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// SoundSurroundFactory
//~=============================================================================

#pragma once
#include "SoundMixFactory.generated.h"

UCLASS(MinimalAPI, hidecategories=Object)
class USoundMixFactory : public UFactory
{
	GENERATED_UCLASS_BODY()


	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	
};
