// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"

#include "CompilerResultsLog.h"
#include "K2Node_CallDataTableFunction.h"

UK2Node_CallDataTableFunction::UK2Node_CallDataTableFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_CallDataTableFunction::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	FString DataTablePinName = GetTargetFunction()->GetMetaData(FBlueprintMetadata::MD_DataTablePin);
	if (Pin->PinName == DataTablePinName)
	{
		// When the DataTable pin gets a new value assigned, we need to update the Slate UI so that the RowName drop down gets updated
		GetGraph()->NotifyGraphChanged();
	}
}

void UK2Node_CallDataTableFunction::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	FString DataTablePinName = GetTargetFunction()->GetMetaData(FBlueprintMetadata::MD_DataTablePin);
	if (Pin->PinName == DataTablePinName)
	{
		// When the DataTable pin gets a new connection assigned, we need to update the Slate UI so that the RowName drop down gets updated
		GetGraph()->NotifyGraphChanged();
	}
}

