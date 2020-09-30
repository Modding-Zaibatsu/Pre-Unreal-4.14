// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphPinString : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinString) {}
	SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	FString GetFormattedTypeInValue() const;
	FText GetTypeInValue() const;
	virtual void SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type CommitInfo);

	/** @return True if the pin default value field is read-only */
	bool GetDefaultValueIsReadOnly() const;
};
