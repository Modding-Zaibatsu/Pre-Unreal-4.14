// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
class SVisualLoggerReport : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVisualLoggerReport) {}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TArray< TSharedPtr<class SLogVisualizerTimeline> >&, TSharedPtr<class SVisualLoggerView> VisualLoggerView);

	virtual ~SVisualLoggerReport();

protected:
	void GenerateReportText();

protected:
	TArray< TSharedPtr<class SLogVisualizerTimeline> > SelectedItems;
	TArray<TSharedRef<ITextDecorator>> Decorators;
	TSharedPtr<class SRichTextBlock> InteractiveRichText;
	FText ReportText;
	TArray<FString> CollectedEvents;
};
