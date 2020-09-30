// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphPinBool : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinBool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	/** Determine if the check box should be checked or not */
	ECheckBoxState IsDefaultValueChecked() const;

	/** Called when check box is changed */
	void OnDefaultValueCheckBoxChanged( ECheckBoxState InIsChecked );
};
