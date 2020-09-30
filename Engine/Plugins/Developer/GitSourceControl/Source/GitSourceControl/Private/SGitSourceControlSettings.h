// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGitSourceControlSettings : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SGitSourceControlSettings) {}
	
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs);

private:

	/** Delegate to get binary path from settings */
	FText GetBinaryPathText() const;

	/** Delegate to commit repository text to settings */
	void OnBinaryPathTextCommited(const FText& InText, ETextCommit::Type InCommitType) const;

	/** Delegate to get repository root, user name and email from provider */
	FText GetPathToRepositoryRoot() const;
	FText GetUserName() const;
	FText GetUserEmail() const;

	/** Delegate to initialize a new Git repository */
	EVisibility CanInitializeGitRepository() const;
	FReply OnClickedInitializeGitRepository();

	void OnCheckedCreateGitIgnore(ECheckBoxState NewCheckedState);
	bool bAutoCreateGitIgnore;

	void OnCheckedInitialCommit(ECheckBoxState NewCheckedState);
	bool bAutoInitialCommit;
	void OnInitialCommitMessageCommited(const FText& InText, ETextCommit::Type InCommitType);
	FText GetInitialCommitMessage() const;
	FText InitialCommitMessage;
};