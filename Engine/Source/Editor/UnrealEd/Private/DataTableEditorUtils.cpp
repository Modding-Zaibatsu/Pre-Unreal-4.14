// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "DataTableEditorUtils.h"
#include "DataTableUtils.h"
#include "ScopedTransaction.h"
#include "K2Node_GetDataTableRow.h"
#include "Engine/UserDefinedStruct.h"

#define LOCTEXT_NAMESPACE "DataTableEditorUtils"

FDataTableEditorUtils::FDataTableEditorManager& FDataTableEditorUtils::FDataTableEditorManager::Get()
{
	static TSharedRef< FDataTableEditorManager > EditorManager(new FDataTableEditorManager());
	return *EditorManager;
}

bool FDataTableEditorUtils::RemoveRow(UDataTable* DataTable, FName Name)
{
	bool bResult = false;
	if (DataTable && DataTable->RowStruct)
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveDataTableRow", "Remove Data Table Row"));

		BroadcastPreChange(DataTable, EDataTableChangeInfo::RowList);
		DataTable->Modify();
		uint8* RowData = NULL;
		const bool bRemoved = DataTable->RowMap.RemoveAndCopyValue(Name, RowData);
		if (bRemoved && RowData)
		{
			DataTable->RowStruct->DestroyStruct(RowData);
			FMemory::Free(RowData);
			bResult = true;

			// Compact the map so that a subsequent add goes at the end of the table
			DataTable->RowMap.Compact();
		}
		BroadcastPostChange(DataTable, EDataTableChangeInfo::RowList);
	}
	return bResult;
}

uint8* FDataTableEditorUtils::AddRow(UDataTable* DataTable, FName RowName)
{
	if (!DataTable || (RowName == NAME_None) || (DataTable->RowMap.Find(RowName) != NULL) || !DataTable->RowStruct)
	{
		return NULL;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddDataTableRow", "Add Data Table Row"));

	BroadcastPreChange(DataTable, EDataTableChangeInfo::RowList);
	DataTable->Modify();
	// Allocate data to store information, using UScriptStruct to know its size
	uint8* RowData = (uint8*)FMemory::Malloc(DataTable->RowStruct->PropertiesSize);
	DataTable->RowStruct->InitializeStruct(RowData);
	// And be sure to call DestroyScriptStruct later

	if (auto UDStruct = Cast<const UUserDefinedStruct>(DataTable->RowStruct))
	{
		UDStruct->InitializeDefaultValue(RowData);
	}

	// Add to row map
	DataTable->RowMap.Add(RowName, RowData);
	BroadcastPostChange(DataTable, EDataTableChangeInfo::RowList);
	return RowData;
}

bool FDataTableEditorUtils::RenameRow(UDataTable* DataTable, FName OldName, FName NewName)
{
	bool bResult = false;
	if (DataTable)
	{
		const FScopedTransaction Transaction(LOCTEXT("RenameDataTableRow", "Rename Data Table Row"));

		BroadcastPreChange(DataTable, EDataTableChangeInfo::RowList);
		DataTable->Modify();

		uint8* RowData = NULL;
		const bool bValidnewName = (NewName != NAME_None) && !DataTable->RowMap.Find(NewName);
		const bool bRemoved = bValidnewName && DataTable->RowMap.RemoveAndCopyValue(OldName, RowData);
		if (bRemoved)
		{
			DataTable->RowMap.FindOrAdd(NewName) = RowData;
			bResult = true;
		}
		BroadcastPostChange(DataTable, EDataTableChangeInfo::RowList);
	}
	return bResult;
}

bool FDataTableEditorUtils::MoveRow(UDataTable* DataTable, FName RowName, ERowMoveDirection Direction, int32 NumRowsToMoveBy)
{
	if (!DataTable)
	{
		return false;
	}
	
	// Our maps are ordered which is why we can get away with this
	// If we ever change our map implementation, we'll need to preserve this order information in a separate array and 
	// make sure that order dependent code (such as exporting and the data table viewer) use that when dealing with rows
	// This may also require making RowMap private and fixing up all the existing code that references it directly
	TArray<FName> OrderedRowNames;
	DataTable->RowMap.GenerateKeyArray(OrderedRowNames);

	const int32 CurrentRowIndex = OrderedRowNames.IndexOfByKey(RowName);
	if (CurrentRowIndex == INDEX_NONE)
	{
		return false;
	}
	
	// Calculate our new row index, clamped to the available rows
	int32 NewRowIndex = INDEX_NONE;
	switch(Direction)
	{
	case ERowMoveDirection::Up:
		NewRowIndex = FMath::Clamp(CurrentRowIndex - NumRowsToMoveBy, 0, OrderedRowNames.Num() - 1);
		break;

	case ERowMoveDirection::Down:
		NewRowIndex = FMath::Clamp(CurrentRowIndex + NumRowsToMoveBy, 0, OrderedRowNames.Num() - 1);
		break;

	default:
		break;
	}

	if (NewRowIndex == INDEX_NONE)
	{
		return false;
	}

	if (CurrentRowIndex == NewRowIndex)
	{
		// Nothing to do, but not an error
		return true;
	}

	// Swap the order around as requested
	OrderedRowNames.RemoveAt(CurrentRowIndex, 1, false);
	OrderedRowNames.Insert(RowName, NewRowIndex);

	// Build a name -> index map as the KeySort will hit this a lot
	TMap<FName, int32> NamesToNewIndex;
	for (int32 NameIndex = 0; NameIndex < OrderedRowNames.Num(); ++NameIndex)
	{
		NamesToNewIndex.Add(OrderedRowNames[NameIndex], NameIndex);
	}

	const FScopedTransaction Transaction(LOCTEXT("MoveDataTableRow", "Move Data Table Row"));

	BroadcastPreChange(DataTable, EDataTableChangeInfo::RowList);
	DataTable->Modify();

	// Re-sort the map keys to match the new order
	DataTable->RowMap.KeySort([&NamesToNewIndex](const FName& One, const FName& Two) -> bool
	{
		const int32 OneIndex = NamesToNewIndex.FindRef(One);
		const int32 TwoIndex = NamesToNewIndex.FindRef(Two);
		return OneIndex < TwoIndex;
	});

	BroadcastPostChange(DataTable, EDataTableChangeInfo::RowList);

	return true;
}

void FDataTableEditorUtils::BroadcastPreChange(UDataTable* DataTable, EDataTableChangeInfo Info)
{
	FDataTableEditorManager::Get().PreChange(DataTable, Info);
}

void FDataTableEditorUtils::BroadcastPostChange(UDataTable* DataTable, EDataTableChangeInfo Info)
{
	if (DataTable && (EDataTableChangeInfo::RowList == Info))
	{
		for (TObjectIterator<UK2Node_GetDataTableRow> It(RF_Transient | RF_ClassDefaultObject, /** bIncludeDerivedClasses */ true, /** InternalExcludeFlags */ EInternalObjectFlags::PendingKill); It; ++It)
		{
			It->OnDataTableRowListChanged(DataTable);
		}
	}
	FDataTableEditorManager::Get().PostChange(DataTable, Info);
}

void FDataTableEditorUtils::CacheDataTableForEditing(const UDataTable* DataTable, TArray<FDataTableEditorColumnHeaderDataPtr>& OutAvailableColumns, TArray<FDataTableEditorRowListViewDataPtr>& OutAvailableRows)
{
	if (!DataTable || !DataTable->RowStruct)
	{
		OutAvailableColumns.Empty();
		OutAvailableRows.Empty();
		return;
	}

	// First build array of properties
	TArray<const UProperty*> StructProps;
	for (TFieldIterator<const UProperty> It(DataTable->RowStruct); It; ++It)
	{
		const UProperty* Prop = *It;
		check(Prop);
		StructProps.Add(Prop);
	}

	TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FTextBlockStyle& CellTextStyle = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("DataTableEditor.CellText");
	static const float CellPadding = 10.0f;

	// Populate the column data
	OutAvailableColumns.Reset(StructProps.Num());
	for (const UProperty* Prop : StructProps)
	{
		FDataTableEditorColumnHeaderDataPtr CachedColumnData = MakeShareable(new FDataTableEditorColumnHeaderData());
		CachedColumnData->ColumnId = Prop->GetFName();
		CachedColumnData->DisplayName = FText::FromString(DataTableUtils::GetPropertyDisplayName(Prop, Prop->GetName()));
		CachedColumnData->DesiredColumnWidth = FontMeasure->Measure(CachedColumnData->DisplayName, CellTextStyle.Font).X + CellPadding;

		OutAvailableColumns.Add(CachedColumnData);
	}

	// Populate the row data
	OutAvailableRows.Reset(DataTable->RowMap.Num());
	for (auto RowIt = DataTable->RowMap.CreateConstIterator(); RowIt; ++RowIt)
	{
		FDataTableEditorRowListViewDataPtr CachedRowData = MakeShareable(new FDataTableEditorRowListViewData());
		CachedRowData->RowId = RowIt->Key;
		CachedRowData->DisplayName = FText::FromName(RowIt->Key);
		CachedRowData->DesiredRowHeight = FontMeasure->GetMaxCharacterHeight(CellTextStyle.Font);

		CachedRowData->CellData.Reserve(StructProps.Num());
		{
			uint8* RowData = RowIt.Value();
			for (int32 ColumnIndex = 0; ColumnIndex < StructProps.Num(); ++ColumnIndex)
			{
				const UProperty* Prop = StructProps[ColumnIndex];
				FDataTableEditorColumnHeaderDataPtr CachedColumnData = OutAvailableColumns[ColumnIndex];

				const FText CellText = DataTableUtils::GetPropertyValueAsText(Prop, RowData);
				CachedRowData->CellData.Add(CellText);

				const FVector2D CellTextSize = FontMeasure->Measure(CellText, CellTextStyle.Font);

				CachedRowData->DesiredRowHeight = FMath::Max(CachedRowData->DesiredRowHeight, CellTextSize.Y);

				const float CellWidth = CellTextSize.X + CellPadding;
				CachedColumnData->DesiredColumnWidth = FMath::Max(CachedColumnData->DesiredColumnWidth, CellWidth);
			}
		}

		OutAvailableRows.Add(CachedRowData);
	}
}

TArray<UScriptStruct*> FDataTableEditorUtils::GetPossibleStructs()
{
	TArray< UScriptStruct* > RowStructs;
	UScriptStruct* TableRowStruct = FindObjectChecked<UScriptStruct>(ANY_PACKAGE, TEXT("TableRowBase"));
	if (TableRowStruct != NULL)
	{
		// Make combo of table rowstruct options
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Struct = *It;
			// If a child of the table row struct base, but not itself
			const bool bBasedOnTableRowBase = Struct->IsChildOf(TableRowStruct) && (Struct != TableRowStruct);
			const bool bUDStruct = Struct->IsA<UUserDefinedStruct>();
			const bool bValidStruct = (Struct->GetOutermost() != GetTransientPackage());
			if ((bBasedOnTableRowBase || bUDStruct) && bValidStruct)
			{
				RowStructs.Add(Struct);
			}
		}
	}
	return RowStructs;
}

#undef LOCTEXT_NAMESPACE
