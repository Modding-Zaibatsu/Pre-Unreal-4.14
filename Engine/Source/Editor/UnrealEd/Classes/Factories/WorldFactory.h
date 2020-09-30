// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 *
 */

#include "WorldFactory.generated.h"

UCLASS(MinimalAPI)
class UWorldFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	TEnumAsByte<EWorldType::Type> WorldType;
	bool bInformEngineOfWorld;
	ERHIFeatureLevel::Type FeatureLevel;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	virtual FText GetToolTip() const override;
	virtual FString GetToolTipDocumentationPage() const override;
	virtual FString GetToolTipDocumentationExcerpt() const override;
	//~ Begin UFactory Interface	
};

