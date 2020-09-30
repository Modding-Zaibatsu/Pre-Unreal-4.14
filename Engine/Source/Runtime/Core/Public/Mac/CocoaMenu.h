// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"

@interface FCocoaMenu : NSMenu
{
@private
	bool bHighlightingKeyEquivalent;
}
- (bool)isHighlightingKeyEquivalent;
- (bool)highlightKeyEquivalent:(NSEvent *)Event;
@end
