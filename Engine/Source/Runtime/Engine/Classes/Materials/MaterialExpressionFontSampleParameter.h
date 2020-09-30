// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Materials/MaterialExpressionFontSample.h"
#include "MaterialExpressionFontSampleParameter.generated.h"

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionFontSampleParameter : public UMaterialExpressionFontSample
{
	GENERATED_UCLASS_BODY()

	/** name to be referenced when we want to find and set thsi parameter */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFontSampleParameter)
	FName ParameterName;

	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;

	/** The name of the parameter Group to display in MaterialInstance Editor. Default is None group */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFontSampleParameter)
	FName Group;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex, int32 MultiplexIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;
#if WITH_EDITOR
	virtual bool CanRenameNode() const override { return true; }
	virtual FString GetEditableName() const override;
	virtual void SetEditableName(const FString& NewName) override;
#endif
	//~ End UMaterialExpression Interface
	
	/** Return whether this is the named parameter, and fill in its value */
	bool IsNamedParameter(FName InParameterName, UFont*& OutFontValue, int32& OutFontPage) const;

	/**
	*	Sets the default Font if none is set
	*/
	virtual void SetDefaultFont();
	
	ENGINE_API virtual FGuid& GetParameterExpressionId() override
	{
		return ExpressionGUID;
	}

	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
};



