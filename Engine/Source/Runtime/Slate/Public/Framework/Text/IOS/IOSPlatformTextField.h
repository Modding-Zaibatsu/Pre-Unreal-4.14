// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPlatformTextField.h"

#import <UIKit/UIKit.h>

@class SlateTextField;

class FIOSPlatformTextField : public IPlatformTextField
{
public:
	FIOSPlatformTextField();
	virtual ~FIOSPlatformTextField();

	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override;

private:
	SlateTextField* TextField;
};

typedef FIOSPlatformTextField FPlatformTextField;

#if !PLATFORM_TVOS
@interface SlateTextField : NSObject<UIAlertViewDelegate>
{
	TSharedPtr<IVirtualKeyboardEntry> TextWidget;
	FText TextEntry;
}

-(void)show:(TSharedPtr<IVirtualKeyboardEntry>)InTextWidget;

@end
#endif
