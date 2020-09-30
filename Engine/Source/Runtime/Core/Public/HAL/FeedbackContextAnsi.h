// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Misc/App.h"
#include "Misc/FeedbackContext.h"

/*-----------------------------------------------------------------------------
	FFeedbackContextAnsi.
-----------------------------------------------------------------------------*/

//
// Feedback context.
//
class FFeedbackContextAnsi : public FFeedbackContext
{
public:
	// Variables.
	FContextSupplier*	Context;
	FOutputDevice*		AuxOut;

	// Local functions.
	void LocalPrint( const TCHAR* Str )
	{
#if PLATFORM_APPLE || PLATFORM_LINUX
		printf("%s", TCHAR_TO_ANSI(Str));
#elif PLATFORM_WINDOWS
		wprintf(TEXT("%ls"), Str);
#else
		// If this function ever gets more complicated, we could make a PlatformMisc::Printf, and each platform can then 
		// do the right thing. For instance, LocalPrint is OutputDebugString on Windows, which messes up a lot of stuff
		FPlatformMisc::LocalPrint(Str);
#endif
	}

	// Constructor.
	FFeedbackContextAnsi()
	: FFeedbackContext()
	, Context(nullptr)
	, AuxOut(nullptr)
	{}
	void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override
	{
		if (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Warning || Verbosity == ELogVerbosity::Display)
		{
			if( TreatWarningsAsErrors && Verbosity==ELogVerbosity::Warning )
			{
				Verbosity = ELogVerbosity::Error;
			}

			FString Prefix;
			if( Context )
			{
				Prefix = Context->GetContext() + TEXT(" : ");
			}
			FString Format = Prefix + FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V);

			// Only store off the message if running a commandlet.
			if ( IsRunningCommandlet() )
			{
				if (Verbosity == ELogVerbosity::Error)
				{
					AddError(Format);
				}
				else if (Verbosity == ELogVerbosity::Warning)
				{
					AddWarning(Format);
				}
			}
			LocalPrint(*Format);
			LocalPrint( TEXT("\n") );
		}
		else if (Verbosity == ELogVerbosity::SetColor)
		{
		}
		if( !GLog->IsRedirectingTo( this ) )
			GLog->Serialize( V, Verbosity, Category );
		if( AuxOut )
			AuxOut->Serialize( V, Verbosity, Category );
		fflush( stdout );
	}

	virtual bool YesNof( const FText& Question ) override
	{
		if( (GIsClient || GIsEditor) )
		{
			LocalPrint( *Question.ToString() );
			LocalPrint( TEXT(" (Y/N): ") );
			if( ( GIsSilent == true ) || ( FApp::IsUnattended() == true ) )
			{
				LocalPrint( TEXT("Y") );
				return true;
			}
			else
			{
				char InputText[256];
				if (fgets( InputText, sizeof(InputText), stdin ) != nullptr)
				{
					return (InputText[0]=='Y' || InputText[0]=='y');
				}
			}
		}
		return true;
	}

	void SetContext( FContextSupplier* InSupplier ) override
	{
		Context = InSupplier;
	}
};

