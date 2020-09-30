// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "BoolKeyArea.h"
#include "SBoolCurveKeyEditor.h"


/* IKeyArea interface
 *****************************************************************************/

bool FBoolKeyArea::CanCreateKeyEditor()
{
	return true;
}


TSharedRef<SWidget> FBoolKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
	return SNew(SBoolCurveKeyEditor)
		.Sequencer(Sequencer)
		.OwningSection(OwningSection)
		.Curve(&Curve)
		.ExternalValue(ExternalValue);
};

bool FBoolKeyArea::ConvertCurveValueToIntegralType(int32 CurveValue) const
{
	return CurveValue != 0;
}

