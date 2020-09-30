// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SkeletonEditorPrivatePCH.h"
#include "SkeletonTreeManager.h"
#include "EditableSkeleton.h"
#include "ISkeletonTree.h"

FSkeletonTreeManager& FSkeletonTreeManager::Get()
{
	static FSkeletonTreeManager TheManager;
	return TheManager;
}

TSharedRef<ISkeletonTree> FSkeletonTreeManager::CreateSkeletonTree(class USkeleton* InSkeleton, class USkeletalMesh* InSkeletalMesh, const FSkeletonTreeArgs& InSkeletonTreeArgs)
{
	TSharedPtr<FEditableSkeleton> EditableSkeleton = CreateEditableSkeleton(InSkeleton, InSkeletalMesh);
	TSharedPtr<ISkeletonTree> SkeletonTree = EditableSkeleton->CreateSkeletonTree(InSkeletonTreeArgs);

	// compact skeletons that are no longer being edited
	bool bRemoved = false;
	do
	{
		bRemoved = false;
		for (auto Iter = EditableSkeletons.CreateIterator(); Iter; ++Iter)
		{
			TSharedPtr<FEditableSkeleton> CompactionCandidate = Iter.Value().Pin();
			if (!CompactionCandidate.IsValid() || !CompactionCandidate->IsEdited())
			{
				Iter.RemoveCurrent();
				bRemoved = true;
				break;
			}
		}
	}
	while (bRemoved);

	return SkeletonTree.ToSharedRef();
}

TSharedRef<FEditableSkeleton> FSkeletonTreeManager::CreateEditableSkeleton(class USkeleton* InSkeleton, class USkeletalMesh* InSkeletalMesh /*= nullptr*/)
{
	TWeakPtr<FEditableSkeleton>* EditableSkeletonPtr = EditableSkeletons.Find(InSkeleton);
	if (EditableSkeletonPtr == nullptr || !(*EditableSkeletonPtr).IsValid())
	{
		TSharedRef<FEditableSkeleton> NewEditableSkeleton = MakeShareable(new FEditableSkeleton(InSkeleton, InSkeletalMesh));
		EditableSkeletons.Add(InSkeleton, NewEditableSkeleton);
		return NewEditableSkeleton;
	}
	else
	{
		return (*EditableSkeletonPtr).Pin().ToSharedRef();
	}
}
