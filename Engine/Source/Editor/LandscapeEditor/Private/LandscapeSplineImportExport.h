// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories.h"

class FLandscapeSplineTextObjectFactory : protected FCustomizableTextObjectFactory
{
public:
	FLandscapeSplineTextObjectFactory(FFeedbackContext* InWarningContext = GWarn);

	TArray<UObject*> ImportSplines(UObject* InParent, const TCHAR* TextBuffer);

protected:
	TArray<UObject*> OutObjects;

	virtual void ProcessConstructedObject(UObject* CreatedObject) override;
	virtual bool CanCreateClass(UClass* ObjectClass, bool& bOmitSubObjs) const override;
};
