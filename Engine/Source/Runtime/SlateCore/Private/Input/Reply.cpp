// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"

FReply& FReply::SetUserFocus(TSharedRef<SWidget> GiveMeFocus, EFocusCause ReasonFocusIsChanging, bool bInAllUsers)
{
	this->bSetUserFocus = true;
	this->FocusRecipient = GiveMeFocus;
	this->FocusChangeReason = ReasonFocusIsChanging;
	this->bReleaseUserFocus = false;
	this->bAllUsers = bInAllUsers;
	return Me();
}

FReply& FReply::ClearUserFocus(EFocusCause ReasonFocusIsChanging, bool bInAllUsers)
{
	this->FocusRecipient = nullptr;
	this->FocusChangeReason = ReasonFocusIsChanging;
	this->bReleaseUserFocus = true;
	this->bSetUserFocus = false;
	this->bAllUsers = bInAllUsers;
	return Me();
}