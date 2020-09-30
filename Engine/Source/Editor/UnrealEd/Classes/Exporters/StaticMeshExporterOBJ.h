// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// StaticMeshExporterOBJ
//~=============================================================================

#pragma once
#include "StaticMeshExporterOBJ.generated.h"

UCLASS()
class UStaticMeshExporterOBJ : public UExporter
{
	GENERATED_UCLASS_BODY()


	//~ Begin UExporter Interface
	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) override;
	//~ End UExporter Interface
};



