// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NullSourceCodeAccessPrivatePCH.h"
#include "NullSourceCodeAccessor.h"
#include "DesktopPlatformModule.h"

#define LOCTEXT_NAMESPACE "NullSourceCodeAccessor"
bool FNullSourceCodeAccessor::CanAccessSourceCode() const
{
	// only check the binaries that UBT will/can use. This file must be in sync with LinuxToolChain.cs
	return FPaths::FileExists(TEXT("/usr/bin/clang++")) || FPaths::FileExists(TEXT("/usr/bin/clang++-3.5"))
		|| FPaths::FileExists(TEXT("/usr/bin/clang++-3.6")) || FPaths::FileExists(TEXT("/usr/bin/clang++-3.7")) || FPaths::FileExists(TEXT("/usr/bin/clang++-3.8"));
}

FName FNullSourceCodeAccessor::GetFName() const
{
	return FName("NullSourceCodeAccessor");
}

FText FNullSourceCodeAccessor::GetNameText() const 
{
	return LOCTEXT("NullDisplayName", "Null Source Code Access");
}

FText FNullSourceCodeAccessor::GetDescriptionText() const
{
	return LOCTEXT("NullDisplayDesc", "Create a c++ project without an IDE installed.");
}

bool FNullSourceCodeAccessor::OpenSolution()
{
	return false;
}

bool FNullSourceCodeAccessor::OpenFileAtLine(const FString& FullPath, int32 LineNumber, int32 ColumnNumber)
{
	return false;
}

bool FNullSourceCodeAccessor::OpenSourceFiles(const TArray<FString>& AbsoluteSourcePaths) 
{
	return false;
}

bool FNullSourceCodeAccessor::AddSourceFiles(const TArray<FString>& AbsoluteSourcePaths, const TArray<FString>& AvailableModules)
{
	return false;
}

bool FNullSourceCodeAccessor::SaveAllOpenDocuments() const
{
	return false;
}

void FNullSourceCodeAccessor::Tick(const float DeltaTime) 
{

}

#undef LOCTEXT_NAMESPACE
