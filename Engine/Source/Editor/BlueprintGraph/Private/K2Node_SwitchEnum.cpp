// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"
#include "Kismet/KismetMathLibrary.h"
#include "K2Node_SwitchEnum.h"
#include "BlueprintFieldNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_SwitchEnum::UK2Node_SwitchEnum(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	bHasDefaultPin = false;

	FunctionName = TEXT("NotEqual_ByteByte");
	FunctionClass = UKismetMathLibrary::StaticClass();
}

void UK2Node_SwitchEnum::SetEnum(UEnum* InEnum)
{
	Enum = InEnum;

	// regenerate enum name list
	EnumEntries.Empty();
	EnumFriendlyNames.Empty();

	if (Enum)
	{
		PreloadObject(Enum);
		for (int32 EnumIndex = 0; EnumIndex < Enum->NumEnums() - 1; ++EnumIndex)
		{
			bool const bShouldBeHidden = Enum->HasMetaData(TEXT("Hidden"), EnumIndex ) || Enum->HasMetaData(TEXT("Spacer"), EnumIndex );
			if (!bShouldBeHidden)
			{
				FString const EnumValueName = Enum->GetEnumName(EnumIndex);
				EnumEntries.Add( FName(*EnumValueName) );

				FText EnumFriendlyName = Enum->GetEnumText(EnumIndex);
				if (EnumFriendlyName.IsEmpty() )
				{
					EnumFriendlyName = FText::FromString(EnumValueName);
				}
				EnumFriendlyNames.Add( EnumFriendlyName );
			}
		}
	}
}

FText UK2Node_SwitchEnum::GetNodeTitle(ENodeTitleType::Type TitleType) const 
{
	if (Enum == nullptr)
	{
		return LOCTEXT("SwitchEnum_BadEnumTitle", "Switch on (bad enum)");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("EnumName"), FText::FromString(Enum->GetName()));
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "Switch_Enum", "Switch on {EnumName}"), Args), this);
	}
	return CachedNodeTitle;
}

FText UK2Node_SwitchEnum::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "SwitchEnum_ToolTip", "Selects an output that matches the input value");
}

bool UK2Node_SwitchEnum::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	const UEnum* SubCategoryObject = Cast<UEnum>( OtherPin->PinType.PinSubCategoryObject.Get() );
	if( SubCategoryObject )
	{
		if( SubCategoryObject != Enum )
		{
			return true;
		}
	}
	return false;
}

void UK2Node_SwitchEnum::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	struct GetMenuActions_Utils
	{
		static void SetNodeEnum(UEdGraphNode* NewNode, UField const* /*EnumField*/, TWeakObjectPtr<UEnum> NonConstEnumPtr)
		{
			UK2Node_SwitchEnum* EnumNode = CastChecked<UK2Node_SwitchEnum>(NewNode);
			EnumNode->Enum = NonConstEnumPtr.Get();
		}
	};

	UClass* NodeClass = GetClass();
	ActionRegistrar.RegisterEnumActions( FBlueprintActionDatabaseRegistrar::FMakeEnumSpawnerDelegate::CreateLambda([NodeClass](const UEnum* InEnum)->UBlueprintNodeSpawner*
	{
		UBlueprintFieldNodeSpawner* NodeSpawner = UBlueprintFieldNodeSpawner::Create(NodeClass, InEnum);
		check(NodeSpawner != nullptr);
		TWeakObjectPtr<UEnum> NonConstEnumPtr = InEnum;
		NodeSpawner->SetNodeFieldDelegate = UBlueprintFieldNodeSpawner::FSetNodeFieldDelegate::CreateStatic(GetMenuActions_Utils::SetNodeEnum, NonConstEnumPtr);

		return NodeSpawner;
	}) );
}

void UK2Node_SwitchEnum::CreateSelectionPin()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* Pin = CreatePin(EGPD_Input, K2Schema->PC_Byte, TEXT(""), Enum, false, false, TEXT("Selection"));
	K2Schema->SetPinDefaultValueBasedOnType(Pin);
}

FEdGraphPinType UK2Node_SwitchEnum::GetPinType() const
{ 
	FEdGraphPinType EnumPinType;
	EnumPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	EnumPinType.PinSubCategoryObject = Enum;
	return EnumPinType; 
}

FString UK2Node_SwitchEnum::GetUniquePinName()
{
	FString NewPinName;
	for (auto EnumIt = EnumEntries.CreateConstIterator(); EnumIt; ++EnumIt)
	{
		FName EnumEntry = *EnumIt;
		if ( !FindPin(EnumEntry.ToString()) )
		{
			break;
		}
	}
	return NewPinName;
}

void UK2Node_SwitchEnum::CreateCasePins()
{
	if(NULL != Enum)
	{
		SetEnum(Enum);
	}

	const bool bShouldUseAdvancedView = (EnumEntries.Num() > 5);
	if(bShouldUseAdvancedView && (ENodeAdvancedPins::NoPins == AdvancedPinDisplay))
	{
		AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	for (auto EnumIt = EnumEntries.CreateConstIterator(); EnumIt; ++EnumIt)
	{
		FName EnumEntry = *EnumIt;
		UEdGraphPin * NewPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, EnumEntry.ToString());
		int32 Index = EnumIt.GetIndex();
		if (EnumFriendlyNames.IsValidIndex(Index))
		{
			NewPin->PinFriendlyName = EnumFriendlyNames[Index];
		}
		
		if(bShouldUseAdvancedView && (EnumIt.GetIndex() > 2))
		{
			NewPin->bAdvancedView = true;
		}
	}
}

UK2Node::ERedirectType UK2Node_SwitchEnum::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	UK2Node::ERedirectType ReturnValue = Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
	if (ReturnValue == UK2Node::ERedirectType_None && Enum && OldPinIndex > 2 && NewPinIndex > 2)
	{
		int32 OldValue = Enum->GetValueByName(FName(*OldPin->PinName));
		int32 NewValue = Enum->GetValueByName(FName(*NewPin->PinName));
		// This handles redirects properly
		if (OldValue == NewValue && OldValue != INDEX_NONE)
		{
			ReturnValue = UK2Node::ERedirectType_Name;
		}
	}
	return ReturnValue;
}

void UK2Node_SwitchEnum::AddPinToSwitchNode()
{
	// first try to restore unconnected pin, since connected one is always visible
	for(auto PinIt = Pins.CreateIterator(); PinIt; ++PinIt)
	{
		UEdGraphPin* Pin = *PinIt;
		if(Pin && (0 == Pin->LinkedTo.Num()) && Pin->bAdvancedView)
		{
			Pin->Modify();
			Pin->bAdvancedView = false;
			return;
		}
	}

	for(auto PinIt = Pins.CreateIterator(); PinIt; ++PinIt)
	{
		UEdGraphPin* Pin = *PinIt;
		if(Pin && Pin->bAdvancedView)
		{
			Pin->Modify();
			Pin->bAdvancedView = false;
			return;
		}
	}
}

void UK2Node_SwitchEnum::RemovePinFromSwitchNode(UEdGraphPin* Pin)
{
	if(Pin)
	{
		if(!Pin->bAdvancedView)
		{
			Pin->Modify();
			Pin->bAdvancedView = true;
		}
		Pin->BreakAllPinLinks();
	}
}

#undef LOCTEXT_NAMESPACE
