// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class IPropertyTableWidgetHandle
{
public: 

	virtual void RequestRefresh() = 0;
	virtual TSharedRef<class SWidget> GetWidget() = 0;

};
