// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class SLATE_API FSlateTextHighlightRunRenderer : public ISlateRunRenderer
{
public:

	static TSharedRef< FSlateTextHighlightRunRenderer > Create();

	virtual ~FSlateTextHighlightRunRenderer() {}

	virtual int32 OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const TSharedRef< ISlateRun >& Run, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

private:

	FSlateTextHighlightRunRenderer();

};
