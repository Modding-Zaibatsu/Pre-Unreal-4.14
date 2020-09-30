// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

struct FSequencerTabSummoner : public FWorkflowTabFactory
{
public:
	static const FName TabID;
	
public:
	FSequencerTabSummoner(TSharedPtr<class FWidgetBlueprintEditor> InBlueprintEditor);
	
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	
protected:
	TWeakPtr<class FWidgetBlueprintEditor> BlueprintEditor;
};
