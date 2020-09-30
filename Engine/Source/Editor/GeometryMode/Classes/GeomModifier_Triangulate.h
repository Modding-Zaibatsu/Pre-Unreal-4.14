// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


/**
 * Triangulates selected objects.
 */

#pragma once
#include "GeomModifier_Triangulate.generated.h"

UCLASS()
class UGeomModifier_Triangulate : public UGeomModifier_Edit
{
	GENERATED_UCLASS_BODY()


	//~ Begin UGeomModifier Interface
	virtual bool Supports() override;
protected:
	virtual bool OnApply() override;
	//~ End UGeomModifier Interface
};



