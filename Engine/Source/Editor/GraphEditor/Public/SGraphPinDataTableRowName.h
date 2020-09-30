// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SGraphPinNameList.h"
#include "DataTableEditorUtils.h"

class GRAPHEDITOR_API SGraphPinDataTableRowName : public SGraphPinNameList, public FDataTableEditorUtils::INotifyOnDataTableChanged
{
public:
	SLATE_BEGIN_ARGS(SGraphPinDataTableRowName) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, class UDataTable* InDataTable);

	SGraphPinDataTableRowName();
	virtual ~SGraphPinDataTableRowName();

	// FDataTableEditorUtils::INotifyOnDataTableChanged
	virtual void PreChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info) override;
	virtual void PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info) override;

protected:

	void RefreshNameList();

	TAssetPtr<class UDataTable> DataTable;
};
