// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KismetCompiler.h"

class FUserDefinedStructureCompilerUtils
{
public:
	// ASSUMPTION, structure doesn't need to be renamed or removed
	static void CompileStruct(class UUserDefinedStruct* Struct, class FCompilerResultsLog& MessageLog, bool bForceRecompile);

	// Fill all variables of given object, that are UserDefinedStructure type, with default values.
	// Default values for member variables in User Defined Structure are stored in meta data "MakeStructureDefaultValue"
	static void DefaultUserDefinedStructs(UObject* Object, FCompilerResultsLog& MessageLog);
};

