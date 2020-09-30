// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GitSourceControlPrivatePCH.h"
#include "GitSourceControlRevision.h"
#include "GitSourceControlModule.h"
#include "GitSourceControlProvider.h"
#include "GitSourceControlUtils.h"
#include "SGitSourceControlSettings.h"

#define LOCTEXT_NAMESPACE "GitSourceControl"

bool FGitSourceControlRevision::Get( FString& InOutFilename ) const
{
	FGitSourceControlModule& GitSourceControl = FModuleManager::LoadModuleChecked<FGitSourceControlModule>("GitSourceControl");
	const FString PathToGitBinary = GitSourceControl.AccessSettings().GetBinaryPath();
	const FString PathToRepositoryRoot = GitSourceControl.GetProvider().GetPathToRepositoryRoot();

	// if a filename for the temp file wasn't supplied generate a unique-ish one
	if(InOutFilename.Len() == 0)
	{
		// create the diff dir if we don't already have it (Git wont)
		IFileManager::Get().MakeDirectory(*FPaths::DiffDir(), true);
		// create a unique temp file name based on the unique commit Id
		const FString TempFileName = FString::Printf(TEXT("%stemp-%s-%s"), *FPaths::DiffDir(), *CommitId, *FPaths::GetCleanFilename(Filename));
		InOutFilename = FPaths::ConvertRelativePathToFull(TempFileName);
	}

	// Diff against the revision
	const FString Parameter = FString::Printf(TEXT("%s:%s"), *CommitId, *Filename);

	bool bCommandSuccessful;
	if(FPaths::FileExists(InOutFilename))
	{
		bCommandSuccessful = true; // if the temp file already exists, reuse it directly
	}
	else
	{
		bCommandSuccessful = GitSourceControlUtils::RunDumpToFile(PathToGitBinary, PathToRepositoryRoot, Parameter, InOutFilename);
	}
	return bCommandSuccessful;
}

bool FGitSourceControlRevision::GetAnnotated( TArray<FAnnotationLine>& OutLines ) const
{
	return false;
}

bool FGitSourceControlRevision::GetAnnotated( FString& InOutFilename ) const
{
	return false;
}

const FString& FGitSourceControlRevision::GetFilename() const
{
	return Filename;
}

int32 FGitSourceControlRevision::GetRevisionNumber() const
{
	return RevisionNumber;
}

const FString& FGitSourceControlRevision::GetRevision() const
{
	return ShortCommitId;
}

const FString& FGitSourceControlRevision::GetDescription() const
{
	return Description;
}

const FString& FGitSourceControlRevision::GetUserName() const
{
	return UserName;
}

const FString& FGitSourceControlRevision::GetClientSpec() const
{
	static FString EmptyString(TEXT(""));
	return EmptyString;
}

const FString& FGitSourceControlRevision::GetAction() const
{
	return Action;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FGitSourceControlRevision::GetBranchSource() const
{
	// @todo if this revision was copied from some other revision, then that source revision should
	//       be returned here (this should be determined when history is being fetched)
	return nullptr;
}

const FDateTime& FGitSourceControlRevision::GetDate() const
{
	return Date;
}

int32 FGitSourceControlRevision::GetCheckInIdentifier() const
{
	// in Git, revisions apply to the whole repository so (in Perforce terms) the revision *is* the changelist
	return RevisionNumber;
}

int32 FGitSourceControlRevision::GetFileSize() const
{
	return FileSize;
}

#undef LOCTEXT_NAMESPACE
