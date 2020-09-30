// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node.h"
#include "K2Node_ExecutionSequence.generated.h"

UCLASS(MinimalAPI)
class UK2Node_ExecutionSequence : public UK2Node
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool CanEverRemoveExecutionPin() const override { return true; }
	//~ End UK2Node Interface

	//~ Begin K2Node_ExecutionSequence Interface

	/** Gets a unique pin name, the next in the sequence */
	FString GetUniquePinName();

	/**
	 * Adds a new execution pin to an execution node
	 */
	BLUEPRINTGRAPH_API void AddPinToExecutionNode();

	/**
	 * Removes the specified execution pin from an execution node
	 *
	 * @param	TargetPin	The pin to remove from the node
	 */
	BLUEPRINTGRAPH_API void RemovePinFromExecutionNode(UEdGraphPin* TargetPin);

	/** Whether an execution pin can be removed from the node or not */
	BLUEPRINTGRAPH_API bool CanRemoveExecutionPin() const;

	// @todo document
	BLUEPRINTGRAPH_API UEdGraphPin * GetThenPinGivenIndex(int32 Index);

private:
	// Returns the exec output pin name for a given 0-based index
	virtual FString GetPinNameGivenIndex(int32 Index) const;
};

