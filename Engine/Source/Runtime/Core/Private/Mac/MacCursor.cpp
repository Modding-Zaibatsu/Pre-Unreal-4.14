// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "MacCursor.h"
#include "MacWindow.h"
#include "MacApplication.h"

#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hidsystem/IOHIDShared.h>

int32 GMacDisableMouseCoalescing = 1;
static FAutoConsoleVariableRef CVarMacDisableMouseCoalescing(
	TEXT("io.Mac.HighPrecisionDisablesMouseCoalescing"),
	GMacDisableMouseCoalescing,
	TEXT("If set to true then OS X mouse event coalescing will be disabled while using high-precision mouse mode, to send all mouse events to UE4's event handling routines to reduce apparent mouse lag. (Default: True)"));

int32 GMacDisableMouseAcceleration = 0;
static FAutoConsoleVariableRef CVarMacDisableMouseAcceleration(
	TEXT("io.Mac.HighPrecisionDisablesMouseAcceleration"),
	GMacDisableMouseAcceleration,
	TEXT("If set to true then OS X's mouse acceleration curve will be disabled while using high-precision mouse mode (typically used when games capture the mouse) resulting in a linear relationship between mouse movement & on-screen cursor movement. For some pointing devices this will make the cursor very slow. (Default: False)"));

FMacCursor::FMacCursor()
:	bIsVisible(true)
,	bUseHighPrecisionMode(false)
,	CurrentPosition(FVector2D::ZeroVector)
,	MouseWarpDelta(FVector2D::ZeroVector)
,	MouseScale(FVector2D::UnitVector)
,	bIsPositionInitialised(false)
,	bShouldIgnoreLocking(false)
,	FullScreenWindow(nullptr)
,	HIDInterface(0)
,	SavedAcceleration(0)
{
	SCOPED_AUTORELEASE_POOL;

	// Load up cursors that we'll be using
	for (int32 CursorIndex = 0; CursorIndex < EMouseCursor::TotalCursorCount; ++CursorIndex)
	{
		CursorHandles[CursorIndex] = NULL;

		NSCursor *CursorHandle = NULL;
		switch (CursorIndex)
		{
			case EMouseCursor::None:
			case EMouseCursor::Custom:
				break;

			case EMouseCursor::Default:
				CursorHandle = [NSCursor arrowCursor];
				break;

			case EMouseCursor::TextEditBeam:
				CursorHandle = [NSCursor IBeamCursor];
				break;

			case EMouseCursor::ResizeLeftRight:
				CursorHandle = [NSCursor resizeLeftRightCursor];
				break;

			case EMouseCursor::ResizeUpDown:
				CursorHandle = [NSCursor resizeUpDownCursor];
				break;

			case EMouseCursor::ResizeSouthEast:
			{
				FString Path = FString::Printf(TEXT("%s%sEditor/Slate/Cursor/SouthEastCursor.png"), FPlatformProcess::BaseDir(), *FPaths::EngineContentDir());
				NSImage* CursorImage = [[NSImage alloc] initWithContentsOfFile:Path.GetNSString()];
				CursorHandle = [[NSCursor alloc] initWithImage:CursorImage hotSpot:NSMakePoint(8, 8)];
				[CursorImage release];
				break;
			}

			case EMouseCursor::ResizeSouthWest:
			{
				FString Path = FString::Printf(TEXT("%s%sEditor/Slate/Cursor/SouthWestCursor.png"), FPlatformProcess::BaseDir(), *FPaths::EngineContentDir());
				NSImage* CursorImage = [[NSImage alloc] initWithContentsOfFile:Path.GetNSString()];
				CursorHandle = [[NSCursor alloc] initWithImage:CursorImage hotSpot:NSMakePoint(8, 8)];
				[CursorImage release];
				break;
			}

			case EMouseCursor::CardinalCross:
			{
				FString Path = FString::Printf(TEXT("%s%sEditor/Slate/Cursor/CardinalCrossCursor.png"), FPlatformProcess::BaseDir(), *FPaths::EngineContentDir());
				NSImage* CursorImage = [[NSImage alloc] initWithContentsOfFile:Path.GetNSString()];
				CursorHandle = [[NSCursor alloc] initWithImage:CursorImage hotSpot:NSMakePoint(8, 8)];
				[CursorImage release];
				break;
			}

			case EMouseCursor::Crosshairs:
				CursorHandle = [NSCursor crosshairCursor];
				break;

			case EMouseCursor::Hand:
				CursorHandle = [NSCursor pointingHandCursor];
				break;

			case EMouseCursor::GrabHand:
				CursorHandle = [NSCursor openHandCursor];
				break;

			case EMouseCursor::GrabHandClosed:
				CursorHandle = [NSCursor closedHandCursor];
				break;

			case EMouseCursor::SlashedCircle:
				CursorHandle = [NSCursor operationNotAllowedCursor];
				break;

			case EMouseCursor::EyeDropper:
			{
				FString Path = FString::Printf(TEXT("%s%sEditor/Slate/Cursor/EyeDropperCursor.png"), FPlatformProcess::BaseDir(), *FPaths::EngineContentDir());
				NSImage* CursorImage = [[NSImage alloc] initWithContentsOfFile:Path.GetNSString()];
				CursorHandle = [[NSCursor alloc] initWithImage:CursorImage hotSpot:NSMakePoint(1, 17)];
				[CursorImage release];
				break;
			}

			default:
				// Unrecognized cursor type!
				check(0);
				break;
		}

		CursorHandles[CursorIndex] = CursorHandle;
	}

	// Set the default cursor
	SetType(EMouseCursor::Default);

	// Get the IOHIDSystem so we can disable mouse acceleration
	mach_port_t MasterPort;
	kern_return_t KernResult = IOMasterPort(MACH_PORT_NULL, &MasterPort);
	if (KERN_SUCCESS == KernResult)
	{
		CFMutableDictionaryRef ClassesToMatch = IOServiceMatching("IOHIDSystem");
		if (ClassesToMatch)
		{
			io_iterator_t MatchingServices;
			KernResult = IOServiceGetMatchingServices(MasterPort, ClassesToMatch, &MatchingServices);
			if (KERN_SUCCESS == KernResult)
			{
				io_object_t IntfService;
				if ((IntfService = IOIteratorNext(MatchingServices)))
				{
					KernResult = IOServiceOpen(IntfService, mach_task_self(), kIOHIDParamConnectType, &HIDInterface);
					if (KernResult == KERN_SUCCESS)
					{
						KernResult = IOHIDGetAccelerationWithKey(HIDInterface, CFSTR(kIOHIDMouseAccelerationType), &SavedAcceleration);
					}
					if (KERN_SUCCESS != KernResult)
					{
						HIDInterface = 0;
					}
				}
			}
		}
	}
}

FMacCursor::~FMacCursor()
{
	SCOPED_AUTORELEASE_POOL;
	
	SetHighPrecisionMouseMode(false);
	
	// Release cursors
	// NOTE: Shared cursors will automatically be destroyed when the application is destroyed.
	//       For dynamically created cursors, use [CursorHandles[CursorIndex] release];
	for (int32 CursorIndex = 0; CursorIndex < EMouseCursor::TotalCursorCount; ++CursorIndex)
	{
		switch (CursorIndex)
		{
			case EMouseCursor::None:
			case EMouseCursor::Default:
			case EMouseCursor::TextEditBeam:
			case EMouseCursor::ResizeLeftRight:
			case EMouseCursor::ResizeUpDown:
			case EMouseCursor::Crosshairs:
			case EMouseCursor::Hand:
			case EMouseCursor::GrabHand:
			case EMouseCursor::GrabHandClosed:
			case EMouseCursor::SlashedCircle:
				// Standard shared cursors don't need to be destroyed
				break;

			case EMouseCursor::ResizeSouthEast:
			case EMouseCursor::ResizeSouthWest:
			case EMouseCursor::CardinalCross:
			case EMouseCursor::EyeDropper:
			case EMouseCursor::Custom:
				if (CursorHandles[CursorIndex] != NULL)
				{
					[CursorHandles[CursorIndex] release];
				}
				break;

			default:
				// Unrecognized cursor type!
				check(0);
				break;
		}
	}
}

void FMacCursor::SetCustomShape(NSCursor* CursorHandle)
{
	SCOPED_AUTORELEASE_POOL;
	[CursorHandle retain];
	if (CursorHandles[EMouseCursor::Custom] != NULL)
	{
		[CursorHandles[EMouseCursor::Custom] release];
	}
	CursorHandles[EMouseCursor::Custom] = CursorHandle;
}

FVector2D FMacCursor::GetPosition() const
{
	FVector2D CurrentPos = CurrentPosition;
	if (!bIsPositionInitialised)
	{
		SCOPED_AUTORELEASE_POOL;
		NSPoint CursorPos = [NSEvent mouseLocation];
		CurrentPos = FMacApplication::ConvertCocoaPositionToSlate(CursorPos.x, CursorPos.y);
	}

	static bool bWasFullscreen = false;

	const FVector2D& MouseScaling = GetMouseScaling();
	if (FullScreenWindow && MouseScaling != FVector2D::UnitVector)
	{
		const FVector2D ScreenOrigin = FMacApplication::CalculateScreenOrigin(FullScreenWindow.screen);
		const FVector2D PositionOnScreen = ((CurrentPos - ScreenOrigin) * MouseScaling) + ScreenOrigin;
		bWasFullscreen = true;
		return PositionOnScreen;
	}
	else
	{
		return CurrentPos;
	}
}

FVector2D FMacCursor::GetPositionNoScaling() const
{
	FVector2D CurrentPos = CurrentPosition;
	if (!bIsPositionInitialised)
	{
		SCOPED_AUTORELEASE_POOL;
		NSPoint CursorPos = [NSEvent mouseLocation];
		CurrentPos = FMacApplication::ConvertCocoaPositionToSlate(CursorPos.x, CursorPos.y);
	}
	return CurrentPos;
}

void FMacCursor::SetPosition(const int32 X, const int32 Y)
{
	const FVector2D& MouseScaling = GetMouseScaling();
	if (FullScreenWindow && MouseScaling != FVector2D::UnitVector)
	{
		const FVector2D ScreenOrigin = FMacApplication::CalculateScreenOrigin(FullScreenWindow.screen);
		const FVector2D PositionOnScreen = ((FVector2D(X, Y) - ScreenOrigin) / MouseScaling) + ScreenOrigin;
		SetPositionNoScaling(PositionOnScreen.X, PositionOnScreen.Y);
	}
	else
	{
		SetPositionNoScaling(X, Y);
	}
}

void FMacCursor::SetPositionNoScaling(const int32 X, const int32 Y)
{
	FVector2D NewPos(X, Y);
	UpdateCursorClipping(NewPos);

	MouseWarpDelta += (NewPos - CurrentPosition);

	if (!bIsPositionInitialised || FIntVector(NewPos.X, NewPos.Y, 0) != FIntVector(CurrentPosition.X, CurrentPosition.Y, 0))
	{
		if (!bUseHighPrecisionMode || (CurrentCursor && bIsVisible))
		{
			WarpCursor(NewPos.X, NewPos.Y);
		}
		else
		{
			UpdateCurrentPosition(NewPos);
		}
	}
}

void FMacCursor::SetType(const EMouseCursor::Type InNewCursor)
{
	check(InNewCursor < EMouseCursor::TotalCursorCount);
	if (MacApplication && CurrentType == EMouseCursor::None && InNewCursor != EMouseCursor::None)
	{
		MacApplication->SetHighPrecisionMouseMode(false, nullptr);
	}
	CurrentType = InNewCursor;
	CurrentCursor = CursorHandles[InNewCursor];
	if (CurrentCursor)
	{
		[CurrentCursor set];
	}
	UpdateVisibility();
}

void FMacCursor::GetSize(int32& Width, int32& Height) const
{
	Width = 16;
	Height = 16;
}

void FMacCursor::Show(bool bShow)
{
	bIsVisible = bShow;
	UpdateVisibility();
}

void FMacCursor::Lock(const RECT* const Bounds)
{
	SCOPED_AUTORELEASE_POOL;

	// Lock/Unlock the cursor
	if (Bounds == nullptr || bShouldIgnoreLocking)
	{
		CursorClipRect = FIntRect();
	}
	else
	{
		CursorClipRect.Min.X = FMath::TruncToInt(Bounds->left);
		CursorClipRect.Min.Y = FMath::TruncToInt(Bounds->top);
		CursorClipRect.Max.X = FMath::TruncToInt(Bounds->right) - 1;
		CursorClipRect.Max.Y = FMath::TruncToInt(Bounds->bottom) - 1;
	}

	MacApplication->OnCursorLock();

	FVector2D Position = GetPositionNoScaling();
	if (UpdateCursorClipping(Position))
	{
		SetPositionNoScaling(Position.X, Position.Y);
	}
}

bool FMacCursor::UpdateCursorClipping(FVector2D& CursorPosition)
{
	bool bAdjusted = false;

	if (CursorClipRect.Area() > 0)
	{
		FVector2D PositionOnScreen(CursorPosition);
		FIntRect ClipRect(CursorClipRect);
		FVector2D ScreenOrigin(FVector2D::ZeroVector);

		const FVector2D& MouseScaling = GetMouseScaling();
		if (FullScreenWindow && MouseScaling != FVector2D::UnitVector)
		{
			ScreenOrigin = FMacApplication::CalculateScreenOrigin(FullScreenWindow.screen);
			PositionOnScreen -= ScreenOrigin;
			int32 ClipRectWidth = (ClipRect.Max.X - ClipRect.Min.X + 1) / MouseScaling.X;
			int32 ClipRectHeight = (ClipRect.Max.Y - ClipRect.Min.Y + 1) / MouseScaling.Y;
			ClipRect.Min.X -= ScreenOrigin.X;
			ClipRect.Min.Y -= ScreenOrigin.Y;
			ClipRect.Max.X = ClipRect.Min.X + ClipRectWidth - 1;
			ClipRect.Max.Y = ClipRect.Min.Y + ClipRectHeight - 1;
		}

		if (PositionOnScreen.X < ClipRect.Min.X)
		{
			PositionOnScreen.X = ClipRect.Min.X;
			bAdjusted = true;
		}
		else if (PositionOnScreen.X > ClipRect.Max.X)
		{
			PositionOnScreen.X = ClipRect.Max.X;
			bAdjusted = true;
		}

		if (PositionOnScreen.Y < ClipRect.Min.Y)
		{
			PositionOnScreen.Y = ClipRect.Min.Y;
			bAdjusted = true;
		}
		else if (PositionOnScreen.Y > ClipRect.Max.Y)
		{
			PositionOnScreen.Y = ClipRect.Max.Y;
			bAdjusted = true;
		}

		if (bAdjusted)
		{
			CursorPosition = PositionOnScreen + ScreenOrigin;
		}
	}

	return bAdjusted;
}

void FMacCursor::UpdateVisibility()
{
	SCOPED_AUTORELEASE_POOL;
	if ([NSApp isActive])
	{
		// @TODO: Remove usage of deprecated CGCursorIsVisible function
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		if (CurrentCursor && bIsVisible)
		{
			// Enable the cursor.
			if (!CGCursorIsVisible())
			{
				CGDisplayShowCursor(kCGDirectMainDisplay);
			}
			if (GMacDisableMouseAcceleration && HIDInterface && bUseHighPrecisionMode)
			{
				IOHIDSetAccelerationWithKey(HIDInterface, CFSTR(kIOHIDMouseAccelerationType), SavedAcceleration);
			}
		}
		else
		{
			// Disable the cursor.
			if (CGCursorIsVisible())
			{
				CGDisplayHideCursor(kCGDirectMainDisplay);
			}
			if (GMacDisableMouseAcceleration && HIDInterface && bUseHighPrecisionMode && (!CurrentCursor || !bIsVisible))
			{
				IOHIDSetAccelerationWithKey(HIDInterface, CFSTR(kIOHIDMouseAccelerationType), -1);
			}
		}
#pragma clang diagnostic pop
	}
	else if (GMacDisableMouseAcceleration && HIDInterface && bUseHighPrecisionMode && (!CurrentCursor || !bIsVisible))
	{
		IOHIDSetAccelerationWithKey(HIDInterface, CFSTR(kIOHIDMouseAccelerationType), SavedAcceleration);
	}
}

void FMacCursor::UpdateCurrentPosition(const FVector2D &Position)
{
	CurrentPosition = Position;
	bIsPositionInitialised = true;
}

void FMacCursor::WarpCursor(const int32 X, const int32 Y)
{
	// Apple suppress mouse events for 0.25 seconds after a call to Warp, unless we call CGAssociateMouseAndMouseCursorPosition.
	// Previously there was CGSetLocalEventsSuppressionInterval to explicitly control this behaviour but that is deprecated.
	// The replacement CGEventSourceSetLocalEventsSuppressionInterval isn't useful because it is unclear how to obtain the correct event source.
	// Instead, when we want the warp to be visible we need to disassociate mouse & cursor...
	if (!bUseHighPrecisionMode)
	{
		CGAssociateMouseAndMouseCursorPosition(false);
	}

	// Perform the warp as normal
	CGWarpMouseCursorPosition(FMacApplication::ConvertSlatePositionToCGPoint(X, Y));

	// And then reassociate the mouse cursor, which forces the mouse events to come through.
	if (!bUseHighPrecisionMode)
	{
		CGAssociateMouseAndMouseCursorPosition(true);
	}

	UpdateCurrentPosition(FVector2D(X, Y));

	MacApplication->IgnoreMouseMoveDelta();
}

FVector2D FMacCursor::GetMouseWarpDelta()
{
	FVector2D Result = (!bUseHighPrecisionMode || (CurrentCursor && bIsVisible)) ? MouseWarpDelta : FVector2D::ZeroVector;
	MouseWarpDelta = FVector2D::ZeroVector;
	return Result;
}

void FMacCursor::SetHighPrecisionMouseMode(const bool bEnable)
{
	if (bUseHighPrecisionMode != bEnable)
	{
		bUseHighPrecisionMode = bEnable;

		CGAssociateMouseAndMouseCursorPosition(!bUseHighPrecisionMode);

		if (GMacDisableMouseCoalescing)
		{
			SCOPED_AUTORELEASE_POOL;
			[NSEvent setMouseCoalescingEnabled:!bUseHighPrecisionMode];
		}

		if (HIDInterface && GMacDisableMouseAcceleration && (!CurrentCursor || !bIsVisible))
		{
			if (!bUseHighPrecisionMode)
			{
				IOHIDSetAccelerationWithKey(HIDInterface, CFSTR(kIOHIDMouseAccelerationType), SavedAcceleration);
			}
			else
			{
				// Update the current saved acceleration.
				double CurrentSetting = 0;
				IOHIDGetAccelerationWithKey(HIDInterface, CFSTR(kIOHIDMouseAccelerationType), &CurrentSetting);
				
				// Need to check that we aren't picking up an invalid setting from ourselves.
				if (CurrentSetting >= 0 && CurrentSetting <= 3.0001)
				{
					SavedAcceleration = CurrentSetting;
				}

				IOHIDSetAccelerationWithKey(HIDInterface, CFSTR(kIOHIDMouseAccelerationType), -1);
			}
		}

		UpdateVisibility();

		// On disable put the cursor where the user would expect it
		if (!bEnable && (!CurrentCursor || !bIsVisible))
		{
			FVector2D Position = GetPositionNoScaling();
			UpdateCursorClipping(Position);
			WarpCursor(Position.X, Position.Y);
		}
	}
}

void FMacCursor::SetMouseScaling(const FVector2D& Scale, FCocoaWindow* InFullScreenWindow)
{
	MouseScale = Scale;
	FullScreenWindow = InFullScreenWindow;
}

const FVector2D& FMacCursor::GetMouseScaling() const
{
	const NSInteger WindowNumber = [NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0];
	if (FullScreenWindow && WindowNumber == FullScreenWindow.windowNumber)
	{
		return MouseScale;
	}
	else
	{
		return FVector2D::UnitVector;
	}
}
