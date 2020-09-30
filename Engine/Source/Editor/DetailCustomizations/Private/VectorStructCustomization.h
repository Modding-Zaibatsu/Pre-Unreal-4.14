// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MathStructCustomizations.h"


/**
 * Customizes FVector structs.
 */
class FVectorStructCustomization
	: public FMathStructCustomization
{
public:

	/** @return A new instance of this class */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

private:

	/** FMathStructCustomization interface */
	virtual void GetSortedChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, TArray<TSharedRef<IPropertyHandle>>& OutChildren) override;
};
