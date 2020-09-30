// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// TextureExporterTGA
//~=============================================================================

#pragma once
#include "TextureExporterTGA.generated.h"

UCLASS()
class UNREALED_API UTextureExporterTGA : public UExporter
{
	GENERATED_UCLASS_BODY()


	//~ Begin UExporter Interface
	virtual bool SupportsObject(UObject* Object) const override;
	virtual bool ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags=0 ) override;
	//~ End UExporter Interface
};



