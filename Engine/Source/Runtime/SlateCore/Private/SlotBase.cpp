// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"

FSlotBase::FSlotBase()
: Widget( SNullWidget::NullWidget )
{

}

FSlotBase::FSlotBase( const TSharedRef<SWidget>& InWidget )
: Widget( InWidget )
{
	
}

const TSharedPtr<SWidget> FSlotBase::DetachWidget()
{
	if (Widget != SNullWidget::NullWidget)
	{
		const TSharedRef<SWidget> MyExWidget = Widget;
		Widget = SNullWidget::NullWidget;	
		return MyExWidget;
	}
	else
	{
		// Nothing to detach!
		return TSharedPtr<SWidget>();
	}
}

FSlotBase::~FSlotBase()
{
}