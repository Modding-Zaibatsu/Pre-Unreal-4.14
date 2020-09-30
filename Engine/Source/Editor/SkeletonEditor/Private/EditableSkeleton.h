// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IEditableSkeleton.h"
#include "Animation/Skeleton.h"

/** View-model for a skeleton tree */
class FEditableSkeleton : public IEditableSkeleton, public FGCObject, public TSharedFromThis<FEditableSkeleton>
{
public:
	/** String used as a header for text based copy-paste of sockets */
	static const FString SocketCopyPasteHeader;

public:
	FEditableSkeleton(USkeleton* InSkeleton, USkeletalMesh* InSkeletalMesh = nullptr);

	/** IEditableSkeleton interface */
	virtual const USkeleton& GetSkeleton() const override;
	virtual const TArray<class UBlendProfile*>& GetBlendProfiles() const override;
	virtual class UBlendProfile* GetBlendProfile(const FName& InBlendProfileName) override;
	virtual class UBlendProfile* CreateNewBlendProfile(const FName& InBlendProfileName) override;
	virtual void RemoveBlendProfile(UBlendProfile* InBlendProfile) override;
	virtual void SetBlendProfileScale(const FName& InBlendProfileName, const FName& InBoneName, float InNewScale, bool bInRecurse) override;
	virtual USkeletalMeshSocket* DuplicateSocket(const FSelectedSocketInfo& SocketInfoToDuplicate, const FName& NewParentBoneName) override;
	virtual int32 ValidatePreviewAttachedObjects() override;
	virtual int32 DeleteAnimNotifies(const TArray<FName>& InSelectedNotifyNames) override;
	virtual int32 RenameNotify(const FName& NewName, const FName& OldName) override;
	virtual void GetCompatibleAnimSequences(TArray<class FAssetData>& OutAssets) override;
	virtual void RenameSocket(const FName& OldSocketName, const FName& NewSocketName) override;
	virtual void SetSocketParent(const FName& SocketName, const FName& NewParentName) override;
	virtual bool DoesSocketAlreadyExist(const class USkeletalMeshSocket* InSocket, const FText& InSocketName, ESocketParentType SocketParentType) const override;
	virtual bool AddSmartname(const FName& InContainerName, const FName& InNewName, FSmartName& OutSmartName) override;
	virtual void RenameSmartname(const FName& InContainerName, SmartName::UID_Type InNameUid, const FName& InNewName) override;
	virtual void RemoveSmartname(const FName& InContainerName, SmartName::UID_Type InNameUid) override;
	virtual void RemoveSmartnamesAndFixupAnimations(const FName& InContainerName, const TArray<SmartName::UID_Type>& InNameUids) override;
	virtual void SetCurveMetaDataMaterial(const FSmartName& CurveName, bool bOverrideMaterial) override;
	virtual void SetPreviewMesh(class USkeletalMesh* InSkeletalMesh) override;
	virtual void LoadAdditionalPreviewSkeletalMeshes() override;
	virtual void SetAdditionalPreviewSkeletalMeshes(class UPreviewMeshCollection* InSkeletalMeshes) override;
	virtual void RenameRetargetSource(const FName& InOldName, const FName& InNewName) override;
	virtual void AddRetargetSource(const FName& InName, USkeletalMesh* InReferenceMesh) override;
	virtual void DeleteRetargetSources(const TArray<FName>& InRetargetSourceNames) override;
	virtual void RefreshRetargetSources(const TArray<FName>& InRetargetSourceNames) override;
	virtual void RefreshRigConfig() override;
	virtual void SetRigConfig(URig* InRig) override;
	virtual void SetRigBoneMapping(const FName& InNodeName, const FName& InBoneName) override;
	virtual void SetRigBoneMappings(const TMap<FName, FName>& InMappings) override;
	virtual void RemoveUnusedBones() override;
	virtual void UpdateSkeletonReferencePose(USkeletalMesh* InSkeletalMesh) override;
	virtual bool AddSlotGroupName(const FName& InSlotName) override;
	virtual void SetSlotGroupName(const FName& InSlotName, const FName& InGroupName) override;
	virtual void DeleteSlotName(const FName& InSlotName) override;
	virtual void DeleteSlotGroup(const FName& InGroupName) override;
	virtual void RenameSlotName(const FName& InOldSlotName, const FName& InNewSlotName) override;
	virtual FDelegateHandle RegisterOnSmartNameRemoved(const FOnSmartNameRemoved::FDelegate& InOnSmartNameRemoved) override;
	virtual void UnregisterOnSmartNameRemoved(FDelegateHandle InHandle) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/** Wrap USkeleton::SetBoneTranslationRetargetingMode */
	void SetBoneTranslationRetargetingMode(FName InBoneName, EBoneTranslationRetargetingMode::Type NewRetargetingMode);

	/** Wrap USkeleton::GetBoneTranslationRetargetingMode */
	EBoneTranslationRetargetingMode::Type GetBoneTranslationRetargetingMode(FName InBoneName);

	/** Generates a unique socket name from the input name, by changing the FName's number */
	FName GenerateUniqueSocketName(FName InName);

	/** Handle the user pasting sockets */
	void HandlePasteSockets(const FName& InBoneName);

	/** Handles adding a socket to the specified bone (i.e. skeleton, not mesh) */
	USkeletalMeshSocket* HandleAddSocket(const FName& InBoneName);

	/** Function to customize a socket - this essentially copies a socket from the skeleton to the mesh */
	void HandleCustomizeSocket(USkeletalMeshSocket* InSocketToCustomize);

	/** Function to promote a socket - this essentially copies a socket from the mesh to the skeleton */
	void HandlePromoteSocket(USkeletalMeshSocket* InSocketToPromote);

	/** Handle removing all attached assets, optionally keeping a preview scene in sync */
	void HandleRemoveAllAssets(TSharedPtr<class IPersonaPreviewScene> InPreviewScene);

	/** Handle attaching assets to the skeleton or mesh, optionally keeping a preview scene in sync */
	void HandleAttachAssets(const TArray<UObject*>& InObjects, const FName& InAttachToName, bool bAttachToMesh, TSharedPtr<class IPersonaPreviewScene> InPreviewScene);

	/** Handle deleting attached assets, optionally keeping a preview scene in sync */
	void HandleDeleteAttachedAssets(const TArray<FPreviewAttachedObjectPair>& InAttachedObjects, TSharedPtr<class IPersonaPreviewScene> InPreviewScene);

	/** Handle deleting sockets, optionally keeping a preview scene in sync */
	void HandleDeleteSockets(const TArray<FSelectedSocketInfo>& InSocketInfo, TSharedPtr<class IPersonaPreviewScene> InPreviewScene);

	/** Set Bone Translation Retargeting Mode for the passed-in bones and their children. */
	void SetBoneTranslationRetargetingModeRecursive(const TArray<FName>& InBoneNames, EBoneTranslationRetargetingMode::Type NewRetargetingMode);

	/** Sets the blend scale for the selected bones and all of their children */
	void RecursiveSetBlendProfileScales(const FName& InBlendProfileName, const TArray<FName>& InBoneNames, float InScaleToSet);

	/** Create a new skeleton tree to edit this editable skeleton */
	TSharedRef<class ISkeletonTree> CreateSkeletonTree(const struct FSkeletonTreeArgs& InSkeletonTreeArgs);

	/** Create a new blend profile picker to edit this editable skeleton's blend profiles */
	TSharedRef<class SWidget> CreateBlendProfilePicker(const struct FBlendProfilePickerArgs& InArgs);

	/** Check whether we have any widgets editing our data */
	bool IsEdited() const { return SkeletonTrees.Num() > 0 || BlendProfilePickers.Num() > 0; }

	/** Access a const version of the skeletal mesh we are optionally editing */
	class USkeletalMesh* const GetSkeletalMesh() const { return SkeletalMesh;  }

	/** Register for skeleton changes */
	void RegisterOnSkeletonHierarchyChanged(const USkeleton::FOnSkeletonHierarchyChanged& InDelegate);

	/** Unregister for skeleton changes */
	void UnregisterOnSkeletonHierarchyChanged(void* Thing);

	/** Wrap USkeleton::RecreateBoneTree */
	void RecreateBoneTree(USkeletalMesh* NewPreviewMesh);

private:
	/** Helper function for deleting attached objects */
	void DeleteAttachedObjects(FPreviewAssetAttachContainer& AttachedAssets, TSharedPtr<class IPersonaPreviewScene> InPreviewScene);

private:
	/** The skeleton we are editing */
	class USkeleton* Skeleton;

	/** Optionally the skeletal mesh we are editing */
	class USkeletalMesh* SkeletalMesh;

	/** All skeleton tree widgets that are editing this skeleton */
	TArray<TWeakPtr<class SSkeletonTree>> SkeletonTrees;

	/** All blend profile widgets that are editing this skeleton */
	TArray<TWeakPtr<class SBlendProfilePicker>> BlendProfilePickers;

	/** Delegate called when trees need refreshing */
	FSimpleMulticastDelegate OnTreeRefresh;

	/** Delegate called when a smart name is removed */
	FOnSmartNameRemoved OnSmartNameRemoved;
};