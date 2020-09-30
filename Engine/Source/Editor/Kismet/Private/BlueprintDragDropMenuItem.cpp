// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintDragDropMenuItem.h"
#include "BlueprintActionMenuItem.h" // for PerformAction()
#include "BlueprintNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
#include "BPDelegateDragDropAction.h"
#include "BPVariableDragDropAction.h"
#include "BlueprintEditor.h"		// for GetVarIconAndColor()
#include "ObjectEditorUtils.h"
#include "SlateColor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "BlueprintActionFilter.h"	// for FBlueprintActionContext
#include "EditorCategoryUtils.h"
#include "EditorStyleSettings.h"	// for bShowFriendlyNames

#define LOCTEXT_NAMESPACE "BlueprintDragDropMenuItem"
DEFINE_LOG_CATEGORY_STATIC(LogBlueprintDragDropMenuItem, Log, All);

//------------------------------------------------------------------------------
FBlueprintDragDropMenuItem::FBlueprintDragDropMenuItem(FBlueprintActionContext const& Context, UBlueprintNodeSpawner const* SampleAction, int32 MenuGrouping, const FText& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip)
: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, MenuGrouping)
{
	AppendAction(SampleAction);
	check(SampleAction != nullptr);
}

//------------------------------------------------------------------------------
UEdGraphNode* FBlueprintDragDropMenuItem::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, FVector2D const Location, bool bSelectNewNode/* = true*/)
{
	// we shouldn't get here (this should just be used for drag/drop ops), but just in case we do...
	FBlueprintActionMenuItem BlueprintActionItem(GetSampleAction());
	return BlueprintActionItem.PerformAction(ParentGraph, FromPin, Location, bSelectNewNode);
}

//------------------------------------------------------------------------------
UEdGraphNode* FBlueprintDragDropMenuItem::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, FVector2D const Location, bool bSelectNewNode/* = true*/)
{
	UEdGraphPin* FromPin = nullptr;
	if (FromPins.Num() > 0)
	{
		FromPin = FromPins[0];
	}
	
	UEdGraphNode* SpawnedNode = PerformAction(ParentGraph, FromPin, Location, bSelectNewNode);
	// try auto-wiring the rest of the pins (if there are any)
	for (int32 PinIndex = 1; PinIndex < FromPins.Num(); ++PinIndex)
	{
		SpawnedNode->AutowireNewNode(FromPins[PinIndex]);
	}
	
	return SpawnedNode;
}

//------------------------------------------------------------------------------
void FBlueprintDragDropMenuItem::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdGraphSchemaAction::AddReferencedObjects(Collector);
	
	// these don't get saved to disk, but we want to make sure the objects don't
	// get GC'd while the action array is around
	Collector.AddReferencedObjects(ActionSet);
}

//------------------------------------------------------------------------------
void FBlueprintDragDropMenuItem::AppendAction(UBlueprintNodeSpawner const* Action)
{
	ActionSet.Add(Action);
}

//------------------------------------------------------------------------------
FSlateBrush const* FBlueprintDragDropMenuItem::GetMenuIcon(FSlateColor& ColorOut)
{
	FSlateBrush const* IconBrush = nullptr;
	ColorOut = FSlateColor(FLinearColor::White);

	UBlueprintNodeSpawner const* SampleAction = GetSampleAction();
	if (UBlueprintDelegateNodeSpawner const* DelegateSpawner = Cast<UBlueprintDelegateNodeSpawner const>(SampleAction))
	{
		IconBrush = FEditorStyle::GetBrush(TEXT("GraphEditor.Delegate_16x"));
	}
	else if (UBlueprintVariableNodeSpawner const* VariableSpawner = Cast<UBlueprintVariableNodeSpawner const>(SampleAction))
	{
		if (UProperty const* Property = VariableSpawner->GetVarProperty())
		{
			UStruct* const PropertyOwner = CastChecked<UStruct>(Property->GetOuterUField());
			// @note(DanO): not sure wht relies on this logic, but discarding extra icon info util
			// I can confirm that containers are a possility here:
			FSlateBrush const* Discarded = nullptr;
			FSlateColor DiscardedColor;
			IconBrush = FBlueprintEditor::GetVarIconAndColor(PropertyOwner, Property->GetFName(), ColorOut, Discarded, DiscardedColor);
		}
	}
	return IconBrush;
}

//------------------------------------------------------------------------------
UBlueprintNodeSpawner const* FBlueprintDragDropMenuItem::GetSampleAction() const
{
	check(ActionSet.Num() > 0);
	return *ActionSet.CreateConstIterator();
}

//------------------------------------------------------------------------------
TSet<UBlueprintNodeSpawner const*> const& FBlueprintDragDropMenuItem::GetActionSet() const
{
	return ActionSet;
}

//------------------------------------------------------------------------------
TSharedPtr<FDragDropOperation> FBlueprintDragDropMenuItem::OnDragged(FNodeCreationAnalytic AnalyticsDelegate) const
{
	TSharedPtr<FDragDropOperation> DragDropAction = nullptr;

	UBlueprintNodeSpawner const* SampleAction = GetSampleAction();
	if (UBlueprintDelegateNodeSpawner const* DelegateSpawner = Cast<UBlueprintDelegateNodeSpawner const>(SampleAction))
	{
		if (UMulticastDelegateProperty const* Property = DelegateSpawner->GetDelegateProperty())
		{
			UStruct* const PropertyOwner = CastChecked<UStruct>(Property->GetOuterUField());
			TSharedRef<FDragDropOperation> DragDropOpRef = FKismetDelegateDragDropAction::New(Property->GetFName(), PropertyOwner, AnalyticsDelegate);
			DragDropAction = TSharedPtr<FDragDropOperation>(DragDropOpRef);
		}
	}
	else if (UBlueprintVariableNodeSpawner const* VariableSpawner = Cast<UBlueprintVariableNodeSpawner const>(SampleAction))
	{
		if (UProperty const* Property = VariableSpawner->GetVarProperty())
		{
			UStruct* const PropertyOwner = CastChecked<UStruct>(Property->GetOuterUField());
			TSharedRef<FDragDropOperation> DragDropOpRef = FKismetVariableDragDropAction::New(Property->GetFName(), PropertyOwner, AnalyticsDelegate);
			DragDropAction = TSharedPtr<FDragDropOperation>(DragDropOpRef);
		}
		// @TODO: handle local variables as well
	}
	else
	{
		UE_LOG(LogBlueprintDragDropMenuItem, Warning, TEXT("Unhandled spawner type: '%s'"), *SampleAction->GetClass()->GetName());
	}
	return DragDropAction;
}

#undef LOCTEXT_NAMESPACE
