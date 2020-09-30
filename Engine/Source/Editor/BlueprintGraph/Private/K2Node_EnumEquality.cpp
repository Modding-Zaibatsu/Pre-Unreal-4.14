// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "BlueprintGraphPrivatePCH.h"
#include "KismetCompiler.h"
#include "Kismet/KismetMathLibrary.h"
#include "K2Node_EnumEquality.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node_EnumEquality"

UK2Node_EnumEquality::UK2Node_EnumEquality(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_EnumEquality::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Create the return value pin
	CreatePin(EGPD_Output, Schema->PC_Boolean, TEXT(""), NULL, false, false, Schema->PN_ReturnValue);

	// Create the input pins
	CreatePin(EGPD_Input, Schema->PC_Wildcard, TEXT(""), NULL, false, false, "A");
	CreatePin(EGPD_Input, Schema->PC_Wildcard, TEXT(""), NULL, false, false, "B");

	Super::AllocateDefaultPins();
}

FText UK2Node_EnumEquality::GetTooltipText() const
{
	return LOCTEXT("EnumEqualityTooltip", "Returns true if A is equal to B (A == B)");
}

FText UK2Node_EnumEquality::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node_EnumEquality", "EqualEnum", "Equal (Enum)");
}

void UK2Node_EnumEquality::PostReconstructNode()
{
	// Do type determination before enum fixup
	PinConnectionListChanged(GetInput1Pin());
	PinConnectionListChanged(GetInput2Pin());

	Super::PostReconstructNode();
// 	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
// 
// 	UEdGraphPin* Input1Pin = GetInput1Pin();
// 	UEdGraphPin* Input2Pin = GetInput2Pin();
// 
// 	if ((Input1Pin->LinkedTo.Num() == 0) && (Input2Pin->LinkedTo.Num() == 0))
// 	{
// 		// Restore the wildcard status
// 		Input1Pin->PinType.PinCategory = Schema->PC_Wildcard;
// 		Input1Pin->PinType.PinSubCategory = TEXT("");
// 		Input1Pin->PinType.PinSubCategoryObject = NULL;
// 		Input2Pin->PinType.PinCategory = Schema->PC_Wildcard;
// 		Input2Pin->PinType.PinSubCategory = TEXT("");
// 		Input2Pin->PinType.PinSubCategoryObject = NULL;
// 	}
}

/** Determine if any pins are connected, if so make all the other pins the same type, if not, make sure pins are switched back to wildcards */
void UK2Node_EnumEquality::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Input1Pin = GetInput1Pin();
	UEdGraphPin* Input2Pin = GetInput2Pin();

	if (Pin == Input1Pin || Pin == Input2Pin)
	{
		if ((Input1Pin->LinkedTo.Num() == 0) && (Input2Pin->LinkedTo.Num() == 0))
		{
			// Restore the wildcard status
			Input1Pin->PinType.PinCategory = Schema->PC_Wildcard;
			Input1Pin->PinType.PinSubCategory = TEXT("");
			Input1Pin->PinType.PinSubCategoryObject = NULL;
			Input2Pin->PinType.PinCategory = Schema->PC_Wildcard;
			Input2Pin->PinType.PinSubCategory = TEXT("");
			Input2Pin->PinType.PinSubCategoryObject = NULL;
			Schema->SetPinDefaultValueBasedOnType(Input1Pin);
			Schema->SetPinDefaultValueBasedOnType(Input2Pin);
			// We have to refresh the graph to get the enum dropdowns to go away
			GetGraph()->NotifyGraphChanged();
		}
		else if (Pin->LinkedTo.Num() > 0)
		{
			// Make sure the pin is a valid enum
			if (Pin->LinkedTo[0]->PinType.PinCategory == Schema->PC_Byte &&
				Pin->LinkedTo[0]->PinType.PinSubCategoryObject.IsValid() &&
				Pin->LinkedTo[0]->PinType.PinSubCategoryObject.Get()->IsA(UEnum::StaticClass()))
			{
				Pin->PinType = Pin->LinkedTo[0]->PinType;

				UEdGraphPin* OtherPin = (Input1Pin == Pin) ? Input2Pin : Input1Pin;

				// Enforce the type on the other pin
				OtherPin->PinType = Pin->PinType;
				UEdGraphSchema_K2::ValidateExistingConnections(OtherPin);
				// If we copied the pin type to a wildcard we have to refresh the graph to get the enum dropdown
				if (OtherPin->LinkedTo.Num() <= 0 && OtherPin->DefaultValue.IsEmpty())
				{
					Schema->SetPinDefaultValueBasedOnType(OtherPin);
					GetGraph()->NotifyGraphChanged();
				}
			}
			// A valid enum wasn't used to break the links
			else
			{
				Pin->BreakAllPinLinks();
			}
		}
		else if(Pin->DefaultValue.IsEmpty())
		{
			Schema->SetPinDefaultValueBasedOnType(Pin);
		}
	}
}

bool UK2Node_EnumEquality::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();

	bool const bOtherPinIsEnum = (OtherPin->PinType.PinCategory == K2Schema->PC_Byte) &&
		(OtherPin->PinType.PinSubCategoryObject.IsValid()) &&
		(OtherPin->PinType.PinSubCategoryObject.Get()->IsA(UEnum::StaticClass()));


	bool bIsDisallowed = false;
	if (!bOtherPinIsEnum && (MyPin->Direction == EGPD_Input))
	{
		bIsDisallowed = true;
		OutReason = LOCTEXT("InputIsNotEnum", "Cannot use the enum equality operator on anything but enums.").ToString();
	}
	return bIsDisallowed;
}

UEdGraphPin* UK2Node_EnumEquality::GetReturnValuePin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(K2Schema->PN_ReturnValue);
	check(Pin != NULL);
	return Pin;
}

UEdGraphPin* UK2Node_EnumEquality::GetInput1Pin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin("A");
	check(Pin != NULL);
	return Pin;
}

UEdGraphPin* UK2Node_EnumEquality::GetInput2Pin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin("B");
	check(Pin != NULL);
	return Pin;
}

void UK2Node_EnumEquality::GetConditionalFunction(FName& FunctionName, UClass** FunctionClass) const
{
	FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_ByteByte);
	*FunctionClass = UKismetMathLibrary::StaticClass();
}

FNodeHandlingFunctor* UK2Node_EnumEquality::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FNodeHandlingFunctor(CompilerContext);
}

void UK2Node_EnumEquality::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

	// Get the enum equality node and the KismetMathLibrary function info for use when we build those nodes
	FName ConditionalFunctionName = "";
	UClass* ConditionalFunctionClass = NULL;
	GetConditionalFunction(ConditionalFunctionName, &ConditionalFunctionClass);

	// Create the conditional node we're replacing the enum node for
	UK2Node_CallFunction* ConditionalNode = SourceGraph->CreateBlankNode<UK2Node_CallFunction>();
	ConditionalNode->FunctionReference.SetExternalMember(ConditionalFunctionName, ConditionalFunctionClass);
	ConditionalNode->AllocateDefaultPins();
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(ConditionalNode, this);

	// Rewire the enum pins to the new conditional node
	UEdGraphPin* LeftSideConditionalPin = ConditionalNode->FindPinChecked(TEXT("A"));
	UEdGraphPin* RightSideConditionalPin = ConditionalNode->FindPinChecked(TEXT("B"));
	UEdGraphPin* ReturnConditionalPin = ConditionalNode->FindPinChecked(Schema->PN_ReturnValue);
	UEdGraphPin* Input1Pin = GetInput1Pin();
	UEdGraphPin* Input2Pin = GetInput2Pin();
	LeftSideConditionalPin->PinType = Input1Pin->PinType;
	RightSideConditionalPin->PinType = Input2Pin->PinType;
	CompilerContext.MovePinLinksToIntermediate(*Input1Pin, *LeftSideConditionalPin);
	CompilerContext.MovePinLinksToIntermediate(*Input2Pin, *RightSideConditionalPin);
	CompilerContext.MovePinLinksToIntermediate(*GetReturnValuePin(), *ReturnConditionalPin);

	// Break all links to the Select node so it goes away for at scheduling time
	BreakAllNodeLinks();
}

void UK2Node_EnumEquality::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

FText UK2Node_EnumEquality::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Enum);
}

#undef LOCTEXT_NAMESPACE
