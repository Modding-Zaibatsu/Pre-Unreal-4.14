// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "K2Node.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "K2Node_SpawnActorFromClass.generated.h"

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_SpawnActorFromClass : public UK2Node
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual void PostPlacedNewNode() override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetNodeAttributes( TArray<TKeyValuePair<FString, FString>>& OutNodeAttributes ) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	//~ End UK2Node Interface


	/**
	 * Create new pins to show properties on archetype
	 *
	 * @param InClass		Class to make pins for
	 * @param OutClassPins	Any pins created through this function will be added to thist list
	 */
	void CreatePinsForClass(UClass* InClass, TArray<UEdGraphPin*>& OutClassPins);
	/** See if this is a spawn variable pin, or a 'default' pin */
	bool IsSpawnVarPin(UEdGraphPin* Pin);

	/** Get the then output pin */
	UEdGraphPin* GetThenPin() const;
	/** Get the blueprint input pin */	
	UEdGraphPin* GetClassPin(const TArray<UEdGraphPin*>* InPinsToSearch=NULL) const;
	/** Get the world context input pin, can return NULL */	
	UEdGraphPin* GetWorldContextPin() const;
	/** Get the spawn transform input pin */	
	UEdGraphPin* GetSpawnTransformPin() const;
	/** Get the collision handling method input pin */
	UEdGraphPin* GetCollisionHandlingOverridePin() const;
	/** Get the actor owner pin */
	UEdGraphPin* GetOwnerPin() const;
	/** Get the result output pin */
	UEdGraphPin* GetResultPin() const;

	/** Get the class that we are going to spawn, if it's defined as default value */
	UClass* GetClassToSpawn(const TArray<UEdGraphPin*>* InPinsToSearch=NULL) const;

protected:

	void OnClassPinChanged();

	/** Tooltip text for this node. */
	FText NodeTooltip;

	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;

private:
	void MaybeUpdateCollisionPin(TArray<UEdGraphPin*>& OldPins);
};
