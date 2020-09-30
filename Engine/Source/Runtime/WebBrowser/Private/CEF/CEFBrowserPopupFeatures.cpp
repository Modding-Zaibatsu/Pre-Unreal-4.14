// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#if WITH_CEF3
#include "CEFBrowserPopupFeatures.h"

FCEFBrowserPopupFeatures::FCEFBrowserPopupFeatures()
	: X(0)
	, bXSet(false)
	, Y(0)
	, bYSet(false)
	, Width(0)
	, bWidthSet(false)
	, Height(0)
	, bHeightSet(false)
	, bMenuBarVisible(true)
	, bStatusBarVisible(false)
	, bToolBarVisible(true)
	, bLocationBarVisible(true)
	, bScrollbarsVisible(true)
	, bResizable(true)
	, bIsFullscreen(false)
	, bIsDialog(false)
{
}


FCEFBrowserPopupFeatures::FCEFBrowserPopupFeatures( const CefPopupFeatures& PopupFeatures )
{
	X = PopupFeatures.x;
	bXSet = PopupFeatures.xSet ? true : false;
	Y = PopupFeatures.y;
	bYSet = PopupFeatures.ySet ? true : false;
	Width = PopupFeatures.width;
	bWidthSet = PopupFeatures.widthSet ? true : false;
	Height = PopupFeatures.height;
	bHeightSet = PopupFeatures.heightSet ? true : false;
	bMenuBarVisible = PopupFeatures.menuBarVisible ? true : false;
	bStatusBarVisible = PopupFeatures.statusBarVisible ? true : false;
	bToolBarVisible = PopupFeatures.toolBarVisible ? true : false;
	bLocationBarVisible = PopupFeatures.locationBarVisible ? true : false;
	bScrollbarsVisible = PopupFeatures.scrollbarsVisible ? true : false;
	bResizable = PopupFeatures.resizable ? true : false;
	bIsFullscreen = PopupFeatures.fullscreen ? true : false;
	bIsDialog = PopupFeatures.dialog ? true : false;

	int Count = PopupFeatures.additionalFeatures ? cef_string_list_size(PopupFeatures.additionalFeatures) : 0;
	CefString ListValue;

	for(int ListIdx = 0; ListIdx < Count; ListIdx++)
	{
		cef_string_list_value(PopupFeatures.additionalFeatures, ListIdx, ListValue.GetWritableStruct());
		AdditionalFeatures.Add(ListValue.ToWString().c_str());
	}

}

FCEFBrowserPopupFeatures::~FCEFBrowserPopupFeatures()
{
}

int FCEFBrowserPopupFeatures::GetX() const
{
	return X;
}

bool FCEFBrowserPopupFeatures::IsXSet() const
{
	return bXSet;
}

int FCEFBrowserPopupFeatures::GetY() const
{
	return Y;
}

bool FCEFBrowserPopupFeatures::IsYSet() const
{
	return bYSet;
}

int FCEFBrowserPopupFeatures::GetWidth() const
{
	return Width;
}

bool FCEFBrowserPopupFeatures::IsWidthSet() const
{
	return bWidthSet;
}

int FCEFBrowserPopupFeatures::GetHeight() const
{
	return Height;
}

bool FCEFBrowserPopupFeatures::IsHeightSet() const
{
	return bHeightSet;
}

bool FCEFBrowserPopupFeatures::IsMenuBarVisible() const
{
	return bMenuBarVisible;
}

bool FCEFBrowserPopupFeatures::IsStatusBarVisible() const
{
	return bStatusBarVisible;
}

bool FCEFBrowserPopupFeatures::IsToolBarVisible() const
{
	return bToolBarVisible;
}

bool FCEFBrowserPopupFeatures::IsLocationBarVisible() const
{
	return bLocationBarVisible;
}

bool FCEFBrowserPopupFeatures::IsScrollbarsVisible() const
{
	return bScrollbarsVisible;
}

bool FCEFBrowserPopupFeatures::IsResizable() const
{
	return bResizable;
}

bool FCEFBrowserPopupFeatures::IsFullscreen() const
{
	return bIsFullscreen;
}

bool FCEFBrowserPopupFeatures::IsDialog() const
{
	return bIsDialog;
}

TArray<FString> FCEFBrowserPopupFeatures::GetAdditionalFeatures() const
{
	return AdditionalFeatures;
}

#endif
