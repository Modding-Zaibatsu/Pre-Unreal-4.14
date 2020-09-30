// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "K2Node_SwitchString.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"

UK2Node_SwitchString::UK2Node_SwitchString(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	bIsCaseSensitive = false;
	SetupCaseSensitivityFunction();
	FunctionClass = UKismetStringLibrary::StaticClass();
}

void UK2Node_SwitchString::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bIsDirty = false;
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == TEXT("PinNames"))
	{
		bIsDirty = true;
	}
	else if (PropertyName == TEXT("bIsCaseSensitive"))
	{
		SetupCaseSensitivityFunction();

		FunctionClass = UKismetStringLibrary::StaticClass();
		bIsDirty = true;
	}
	 
	if (bIsDirty)
	{
		ReconstructNode();
		GetGraph()->NotifyGraphChanged();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UK2Node_SwitchString::SetupCaseSensitivityFunction()
{
	FunctionName = (bIsCaseSensitive == true)
		?  TEXT("NotEqual_StrStr")
		: TEXT("NotEqual_StriStri");
}

FText UK2Node_SwitchString::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "Switch_String", "Switch on String");
}

FText UK2Node_SwitchString::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "SwitchString_ToolTip", "Selects an output that matches the input value");
}

void UK2Node_SwitchString::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UK2Node_SwitchString::CreateSelectionPin()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* Pin = CreatePin(EGPD_Input, K2Schema->PC_String, TEXT(""), NULL, false, false, TEXT("Selection"));
	K2Schema->SetPinDefaultValueBasedOnType(Pin);
}

FEdGraphPinType UK2Node_SwitchString::GetPinType() const 
{ 
	FEdGraphPinType PinType;
	PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	return PinType;
}

FString UK2Node_SwitchString::GetPinNameGivenIndex(int32 Index)
{
	check(Index);
	return PinNames[Index];
}


void UK2Node_SwitchString::CreateCasePins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	for( TArray<FString>::TIterator it(PinNames); it; ++it )
	{
		CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, *it);
	}
}

FString UK2Node_SwitchString::GetUniquePinName()
{
	FString NewPinName;
	int32 Index = 0;
	while (true)
	{
		NewPinName =  FString::Printf(TEXT("Case_%d"), Index++);
		if (!FindPin(NewPinName))
		{
			break;
		}
	}
	return NewPinName;
}

void UK2Node_SwitchString::AddPinToSwitchNode()
{
	FString PinName = GetUniquePinName();
	PinNames.Add(PinName);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, PinName);
}

void UK2Node_SwitchString::RemovePin(UEdGraphPin* TargetPin)
{
	checkSlow(TargetPin);

	// Clean-up pin name array
	PinNames.Remove(TargetPin->PinName);
}
