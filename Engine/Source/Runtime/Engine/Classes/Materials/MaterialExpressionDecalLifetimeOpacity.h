// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionDecalLifetimeOpacity.generated.h"


UCLASS(collapsecategories, hidecategories = Object)
class UMaterialExpressionDecalLifetimeOpacity : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};