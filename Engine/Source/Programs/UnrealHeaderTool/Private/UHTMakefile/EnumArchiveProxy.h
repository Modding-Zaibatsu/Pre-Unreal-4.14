// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "UHTMakefile/FieldArchiveProxy.h"

/* See UHTMakefile.h for overview how makefiles work. */
struct FEnumArchiveProxy : public FFieldArchiveProxy
{
	FEnumArchiveProxy() { }
	FEnumArchiveProxy(FUHTMakefile& UHTMakefile, const UEnum* Enum);

	UEnum* CreateEnum(const FUHTMakefile& UHTMakefile) const;
	void PostConstruct(UEnum* Enum, const FUHTMakefile& UHTMakefile) const;

	static void AddReferencedNames(const UEnum* Enum, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FEnumArchiveProxy& EnumArchiveProxy);

	uint32 EnumFlags;
	uint32 CppForm;
	FString CppType;
	TArray<TPair<FNameArchiveProxy, uint8>> EnumValues;
};
