// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraScriptFactoryNew.generated.h"

UCLASS(hidecategories=Object)
class UNiagaraScriptFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	
};



