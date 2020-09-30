// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphPin.h"
#include "SGraphPinString.h"
#include "SGraphPinNum.h"
#include "DefaultValueHelper.h"
#include "Editor/UnrealEd/Public/ScopedTransaction.h"

void SGraphPinNum::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPinString::Construct(SGraphPinString::FArguments(), InGraphPinObj);
}

void SGraphPinNum::SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/)
{
	FString TypeValueString = NewTypeInValue.ToString();
	if (FDefaultValueHelper::IsStringValidFloat(TypeValueString) || FDefaultValueHelper::IsStringValidInteger(TypeValueString))
	{
		if(GraphPinObj->GetDefaultAsString() != TypeValueString)
		{
			const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeNumberPinValue", "Change Number Pin Value" ) );
			GraphPinObj->Modify();

			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, *TypeValueString);
		}
	}
}
