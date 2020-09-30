// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SkeletonEditorPrivatePCH.h"
#include "EditableSkeleton.h"
#include "SSkeletonTree.h"
#include "IPersonaToolkit.h"
#include "MeshUtilities.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "AnimPreviewInstance.h"
#include "ScopedTransaction.h"
#include "Factories.h"
#include "Animation/BlendProfile.h"
#include "IPersonaPreviewScene.h"
#include "SBlendProfilePicker.h"
#include "AssetNotifications.h"
#include "BlueprintActionDatabase.h"
#include "AssetRegistryModule.h"
#include "SSkeletonWidget.h"
#include "Animation/DebugSkelMeshComponent.h"

const FString FEditableSkeleton::SocketCopyPasteHeader = TEXT("SocketCopyPasteBuffer");

#define LOCTEXT_NAMESPACE "EditableSkeleton"

/** Constructs sockets from clipboard data */

class FSocketTextObjectFactory : public FCustomizableTextObjectFactory
{
public:
	FSocketTextObjectFactory(USkeleton* InSkeleton, USkeletalMesh* InSkeletalMesh, FName InPasteBone)
		: FCustomizableTextObjectFactory(GWarn)
		, PasteBone(InPasteBone)
		, Skeleton(InSkeleton)
		, SkeletalMesh(InSkeletalMesh)
	{
		check(Skeleton);
	};

	// Pointer back to the outside world that will hold the final imported socket
	TArray<USkeletalMeshSocket*> CreatedSockets;

private:

	virtual bool CanCreateClass(UClass* ObjectClass, bool& bOmitSubObjs) const override { return true; }

	virtual void ProcessConstructedObject(UObject* CreatedObject)
	{
		USkeletalMeshSocket* NewSocket = CastChecked<USkeletalMeshSocket>(CreatedObject);
		CreatedSockets.Add(NewSocket);

		const FReferenceSkeleton* RefSkel = nullptr;

		if (USkeletalMesh* SocketMesh = Cast<USkeletalMesh>(NewSocket->GetOuter()))
		{
			SocketMesh->GetMeshOnlySocketList().Add(NewSocket);
			RefSkel = &SocketMesh->RefSkeleton;
		}
		else if (USkeleton* SocketSkeleton = Cast<USkeleton>(NewSocket->GetOuter()))
		{
			SocketSkeleton->Sockets.Add(NewSocket);
			RefSkel = &SocketSkeleton->GetReferenceSkeleton();
		}
		else
		{
			check(false) //Unknown socket outer
		}

		if (!PasteBone.IsNone())
		{
			// Override the bone name to the one we pasted to
			NewSocket->BoneName = PasteBone;
		}
		else
		{
			//Validate BoneName
			if (RefSkel->FindBoneIndex(NewSocket->BoneName) == INDEX_NONE)
			{
				NewSocket->BoneName = RefSkel->GetBoneName(0);
			}
		}
	}

	virtual void ProcessUnidentifiedLine(const FString& StrLine)
	{
		bool bIsOnSkeleton;
		FParse::Bool(*StrLine, TEXT("IsOnSkeleton="), bIsOnSkeleton);
		bExpectingMeshSocket = !bIsOnSkeleton;
	}

	virtual UObject* GetParentForNewObject(const UClass* ObjClass)
	{
		UObject* Target = (bExpectingMeshSocket && SkeletalMesh) ? (UObject*)SkeletalMesh : (UObject*)Skeleton;
		Target->Modify();
		return Target;
	}

	const FName PasteBone;

	// Track what type of socket we will be processing next
	bool bExpectingMeshSocket;

	// Target for Skeleton Sockets
	USkeleton* Skeleton;

	// Target for Mesh Sockets (could be nullptr)
	USkeletalMesh* SkeletalMesh;
};

FEditableSkeleton::FEditableSkeleton(USkeleton* InSkeleton, USkeletalMesh* InSkeletalMesh)
	: Skeleton(InSkeleton)
	, SkeletalMesh(InSkeletalMesh)
{
	Skeleton->CollectAnimationNotifies();
}

const USkeleton& FEditableSkeleton::GetSkeleton() const
{
	return *Skeleton;
}

const TArray<class UBlendProfile*>& FEditableSkeleton::GetBlendProfiles() const
{
	return Skeleton->BlendProfiles;
}

UBlendProfile* FEditableSkeleton::GetBlendProfile(const FName& NameToUse)
{
	return Skeleton->GetBlendProfile(NameToUse);
}

UBlendProfile* FEditableSkeleton::CreateNewBlendProfile(const FName& NameToUse)
{
	FScopedTransaction Transaction(LOCTEXT("CreateBlendProfile", "Create Blend Profile"));

	return Skeleton->CreateNewBlendProfile(NameToUse);
}

void FEditableSkeleton::RemoveBlendProfile(UBlendProfile* InBlendProfile)
{
	FScopedTransaction Transaction(LOCTEXT("RemoveBlendProfile", "Remove Blend Profile"));

	Skeleton->Modify();
	Skeleton->BlendProfiles.Remove(InBlendProfile);
}

void FEditableSkeleton::SetBlendProfileScale(const FName& InBlendProfileName, const FName& InBoneName, float InNewScale, bool bInRecurse)
{
	UBlendProfile* BlendProfile = GetBlendProfile(InBlendProfileName);
	if (BlendProfile)
	{
		FScopedTransaction Transaction(LOCTEXT("SetBlendProfileScale", "Set Blend Profile Scale"));

		BlendProfile->SetFlags(RF_Transactional);
		BlendProfile->Modify();
		const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(InBoneName);
		BlendProfile->SetBoneBlendScale(BoneIndex, InNewScale, bInRecurse, true);
	}
}

void FEditableSkeleton::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Skeleton);
	Collector.AddReferencedObject(SkeletalMesh);
}

void FEditableSkeleton::SetBoneTranslationRetargetingMode(FName InBoneName, EBoneTranslationRetargetingMode::Type NewRetargetingMode)
{
	const FScopedTransaction Transaction(LOCTEXT("SetBoneTranslationRetargetingMode", "Set Bone Translation Retargeting Mode"));
	Skeleton->Modify();

	const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(InBoneName);
	Skeleton->SetBoneTranslationRetargetingMode(BoneIndex, NewRetargetingMode);
	FAssetNotifications::SkeletonNeedsToBeSaved(Skeleton);
}

EBoneTranslationRetargetingMode::Type FEditableSkeleton::GetBoneTranslationRetargetingMode(FName InBoneName)
{
	const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(InBoneName);
	return Skeleton->GetBoneTranslationRetargetingMode(BoneIndex);
}

bool FEditableSkeleton::DoesSocketAlreadyExist(const USkeletalMeshSocket* InSocket, const FText& InSocketName, ESocketParentType SocketParentType) const
{
	TArray<USkeletalMeshSocket*>* SocketArrayPtr = nullptr;
	if (SocketParentType == ESocketParentType::Mesh && SkeletalMesh)
	{
		SocketArrayPtr = &SkeletalMesh->GetMeshOnlySocketList();
	}
	else if(SocketParentType == ESocketParentType::Skeleton)
	{
		SocketArrayPtr = &Skeleton->Sockets;
	}

	if (SocketArrayPtr != nullptr)
	{
		for (auto SocketIt = SocketArrayPtr->CreateConstIterator(); SocketIt; ++SocketIt)
		{
			USkeletalMeshSocket* Socket = *(SocketIt);

			if (InSocket != Socket && Socket->SocketName.ToString() == InSocketName.ToString())
			{
				return true;
			}
		}
	}

	return false;
}

bool FEditableSkeleton::AddSmartname(const FName& InContainerName, const FName& InNewName, FSmartName& OutSmartName)
{
	if (const FSmartNameMapping* NameMapping = Skeleton->GetSmartNameContainer(InContainerName))
	{
		return Skeleton->AddSmartNameAndModify(InContainerName, InNewName, OutSmartName);
	}

	return false;
}

void FEditableSkeleton::RenameSmartname(const FName& InContainerName, SmartName::UID_Type InNameUid, const FName& InNewName)
{
	const FSmartNameMapping* Mapping = Skeleton->GetSmartNameContainer(InContainerName);
	if (Mapping)
	{
		if (!Mapping->Exists(InNewName))
		{
			FScopedTransaction Transaction(LOCTEXT("TransactionRename", "Rename Element"));
			Skeleton->RenameSmartnameAndModify(InContainerName, InNameUid, InNewName);
		}
	}
}

void FEditableSkeleton::RemoveSmartname(const FName& InContainerName, SmartName::UID_Type InNameUid)
{
	// Note no transaction here as this doesnt do anything at the moment!
	Skeleton->RemoveSmartnameAndModify(InContainerName, InNameUid);
}

void FEditableSkeleton::RemoveSmartnamesAndFixupAnimations(const FName& InContainerName, const TArray<SmartName::UID_Type>& InNameUids)
{
	FAssetRegistryModule& AssetModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> AnimationAssets;
	AssetModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetFName(), AnimationAssets, true);
	FAssetData SkeletonData(Skeleton);
	const FString CurrentSkeletonName = SkeletonData.GetExportTextName();

	GWarn->BeginSlowTask(LOCTEXT("CollectAnimationsTaskDesc", "Collecting assets..."), true);

	for (int32 Idx = AnimationAssets.Num() - 1; Idx >= 0; --Idx)
	{
		FAssetData& Data = AnimationAssets[Idx];
		bool bRemove = true;

		const FString SkeletonDataTag = Data.GetTagValueRef<FString>("Skeleton");
		if (!SkeletonDataTag.IsEmpty() && SkeletonDataTag == CurrentSkeletonName)
		{
			FString CurveData;
			if (!Data.GetTagValue(USkeleton::CurveTag, CurveData))
			{
				// This asset is old; it hasn't been loaded since before smartnames were added for curves.
				// unfortunately the only way to delete curves safely is to load old assets to see if they're
				// using the selected name. We only load what we have to here.
				UObject* Asset = Data.GetAsset();
				check(Asset);
				TArray<UObject::FAssetRegistryTag> Tags;
				Asset->GetAssetRegistryTags(Tags);

				UObject::FAssetRegistryTag* CurveTag = Tags.FindByPredicate([](const UObject::FAssetRegistryTag& InTag)
				{
					return InTag.Name == USkeleton::CurveTag;
				});
				CurveData = CurveTag->Value;
			}

			TArray<FString> ParsedStringUids;
			CurveData.ParseIntoArray(ParsedStringUids, *USkeleton::CurveTagDelimiter, true);

			for (const FString& UidString : ParsedStringUids)
			{
				SmartName::UID_Type Uid = (SmartName::UID_Type)TCString<TCHAR>::Strtoui64(*UidString, NULL, 10);
				if (InNameUids.Contains(Uid))
				{
					bRemove = false;
					break;
				}
			}
		}

		if (bRemove)
		{
			AnimationAssets.RemoveAt(Idx);
		}
	}

	GWarn->EndSlowTask();

	// AnimationAssets now only contains assets that are using the selected curve(s)
	if (AnimationAssets.Num() > 0)
	{
		FString ConfirmMessage = LOCTEXT("DeleteCurveMessage", "The following assets use the selected curve(s), deleting will remove the curves from these assets. Continue?\n\n").ToString();

		for (FAssetData& Data : AnimationAssets)
		{
			ConfirmMessage += Data.AssetName.ToString() + " (" + Data.AssetClass.ToString() + ")\n";
		}

		FText Title = LOCTEXT("DeleteCurveDialogTitle", "Confirm Deletion");
		FText Message = FText::FromString(ConfirmMessage);
		if (FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title) == EAppReturnType::No)
		{
			return;
		}
		else
		{
			// Proceed to delete the curves
			GWarn->BeginSlowTask(FText::Format(LOCTEXT("DeleteCurvesTaskDesc", "Deleting curve from skeleton {0}"), FText::FromString(Skeleton->GetName())), true);
			FScopedTransaction Transaction(LOCTEXT("DeleteCurvesTransactionName", "Delete skeleton curve"));

			// Remove curves from animation assets
			for (FAssetData& Data : AnimationAssets)
			{
				UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Data.GetAsset());
				USkeleton* MySkeleton = Sequence->GetSkeleton();
				Sequence->Modify(true);
				for (SmartName::UID_Type Uid : InNameUids)
				{
					FSmartName CurveToDelete;
					if (MySkeleton->GetSmartNameByUID(USkeleton::AnimCurveMappingName, Uid, CurveToDelete))
					{
						Sequence->RawCurveData.DeleteCurveData(CurveToDelete);
						Sequence->MarkRawDataAsModified();
					}
				}
			}
			GWarn->EndSlowTask();

			GWarn->BeginSlowTask(LOCTEXT("RebuildingAnimations", "Rebaking/compressing modified animations"), true);

			// Rebake/compress the animations
			for (TObjectIterator<UAnimSequence> It; It; ++It)
			{
				UAnimSequence* Seq = *It;

				GWarn->StatusUpdate(1, 2, FText::Format(LOCTEXT("RebuildingAnimationsStatus", "Rebuilding {0}"), FText::FromString(Seq->GetName())));
				Seq->RequestSyncAnimRecompression();
			}
			GWarn->EndSlowTask();
		}
	}

	if (InNameUids.Num() > 0)
	{
		// Remove names from skeleton
		Skeleton->RemoveSmartnamesAndModify(InContainerName, InNameUids);
	}

	OnSmartNameRemoved.Broadcast(InContainerName, InNameUids);
}

void FEditableSkeleton::SetCurveMetaDataMaterial(const FSmartName& CurveName, bool bOverrideMaterial)
{
	FCurveMetaData* CurveMetaData = Skeleton->GetCurveMetaData(CurveName);
	if (CurveMetaData)
	{
		// override curve data
		CurveMetaData->Type.bMaterial = !!bOverrideMaterial;
	}
}

FName FEditableSkeleton::GenerateUniqueSocketName( FName InName )
{
	int32 NewNumber = InName.GetNumber();

	// Increment NewNumber until we have a unique name (potential infinite loop if *all* int32 values are used!)
	while ( DoesSocketAlreadyExist( NULL, FText::FromName( FName( InName, NewNumber ) ), ESocketParentType::Skeleton ) ||
		(SkeletalMesh ? DoesSocketAlreadyExist( NULL, FText::FromName( FName( InName, NewNumber ) ), ESocketParentType::Mesh ) : false ) )
	{
		++NewNumber;
	}

	return FName( InName, NewNumber );
}

static USkeletalMeshSocket* FindSocket(const FName& InSocketName, USkeletalMesh* InSkeletalMesh, USkeleton* InSkeleton)
{
	USkeletalMeshSocket* Socket = nullptr;

	// first check the skeletal mesh as that is the behavior of USkinnedMeshComponent
	if (InSkeletalMesh != nullptr)
	{
		Socket = InSkeletalMesh->FindSocket(InSocketName);
	}

	if (Socket == nullptr && InSkeleton != nullptr)
	{
		Socket = InSkeleton->FindSocket(InSocketName);
	}

	return Socket;
}

void FEditableSkeleton::RenameSocket(const FName& OldSocketName, const FName& NewSocketName)
{
	USkeletalMeshSocket* SocketData = FindSocket(OldSocketName, SkeletalMesh, Skeleton);

	if (SocketData != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("RenameSocket", "Rename Socket"));
		SocketData->SetFlags(RF_Transactional);	// Undo doesn't work without this!
		SocketData->Modify();

		SocketData->SocketName = NewSocketName;
	}

	{
		const FScopedTransaction Transaction(LOCTEXT("RenameSocketAndMoveAttachments", "Rename Socket And Move Attachments"));

		bool bSkeletonModified = false;
		for (int AttachedObjectIndex = 0; AttachedObjectIndex < Skeleton->PreviewAttachedAssetContainer.Num(); ++AttachedObjectIndex)
		{
			FPreviewAttachedObjectPair& Pair = Skeleton->PreviewAttachedAssetContainer[AttachedObjectIndex];
			if (Pair.AttachedTo == OldSocketName)
			{
				// Only modify the skeleton if we actually intend to change something.
				if (!bSkeletonModified)
				{
					Skeleton->Modify();
					bSkeletonModified = true;
				}
				Pair.AttachedTo = NewSocketName;
			}

			// push to skeleton trees
			for (TWeakPtr<SSkeletonTree> SkeletonTree : SkeletonTrees)
			{
				if (SkeletonTree.IsValid())
				{
					SkeletonTree.Pin()->PostRenameSocket(Pair.GetAttachedObject(), OldSocketName, Pair.AttachedTo);
				}
			}
		}

		if (SkeletalMesh != nullptr)
		{
			bool bMeshModified = false;
			for (int AttachedObjectIndex = 0; AttachedObjectIndex < SkeletalMesh->PreviewAttachedAssetContainer.Num(); ++AttachedObjectIndex)
			{
				FPreviewAttachedObjectPair& Pair = SkeletalMesh->PreviewAttachedAssetContainer[AttachedObjectIndex];
				if (Pair.AttachedTo == OldSocketName)
				{
					// Only modify the mesh if we actually intend to change something. Avoids dirtying
					// meshes when we don't actually update any data on them. (such as adding a new socket)
					if (!bMeshModified)
					{
						SkeletalMesh->Modify();
						bMeshModified = true;
					}
					Pair.AttachedTo = NewSocketName;
				}

				for (TWeakPtr<SSkeletonTree> SkeletonTree : SkeletonTrees)
				{
					if (SkeletonTree.IsValid())
					{
						SkeletonTree.Pin()->PostRenameSocket(Pair.GetAttachedObject(), OldSocketName, Pair.AttachedTo);
					}
				}
			}
		}
	}
}

void FEditableSkeleton::SetSocketParent(const FName& SocketName, const FName& NewParentName)
{
	USkeletalMeshSocket* Socket = FindSocket(SocketName, SkeletalMesh, Skeleton);
	if (Socket)
	{
		// Create an undo transaction, re-parent the socket and rebuild the skeleton tree view
		const FScopedTransaction Transaction(LOCTEXT("ReparentSocket", "Re-parent Socket"));

		Socket->SetFlags(RF_Transactional);	// Undo doesn't work without this!
		Socket->Modify();

		Socket->BoneName = NewParentName;

		OnTreeRefresh.Broadcast();
	}
}

void FEditableSkeleton::HandlePasteSockets(const FName& InBoneName)
{
	FString PasteString;
	FPlatformMisc::ClipboardPaste(PasteString);
	const TCHAR* PastePtr = *PasteString;

	FString PasteLine;
	FParse::Line(&PastePtr, PasteLine);

	if (PasteLine == SocketCopyPasteHeader)
	{
		const FScopedTransaction Transaction(LOCTEXT("PasteSockets", "Paste sockets"));

		int32 NumSocketsToPaste;
		FParse::Line(&PastePtr, PasteLine);	// Need this to advance PastePtr, for multiple sockets
		FParse::Value(*PasteLine, TEXT("NumSockets="), NumSocketsToPaste);

		FSocketTextObjectFactory TextObjectFactory(Skeleton, SkeletalMesh, InBoneName);
		TextObjectFactory.ProcessBuffer(nullptr, RF_Transactional, PastePtr);

		for (USkeletalMeshSocket* NewSocket : TextObjectFactory.CreatedSockets)
		{
			// Check the socket name is unique
			NewSocket->SocketName = GenerateUniqueSocketName(NewSocket->SocketName);
		}
	}
}

USkeletalMeshSocket* FEditableSkeleton::HandleAddSocket(const FName& InBoneName)
{
	const FScopedTransaction Transaction(LOCTEXT("AddSocket", "Add Socket to Skeleton"));
	Skeleton->Modify();

	USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(Skeleton);
	check(NewSocket);

	NewSocket->BoneName = InBoneName;
	FString SocketName = NewSocket->BoneName.ToString() + LOCTEXT("SocketPostfix", "Socket").ToString();
	NewSocket->SocketName = GenerateUniqueSocketName(*SocketName);

	Skeleton->Sockets.Add(NewSocket);
	return NewSocket;
}

void FEditableSkeleton::HandleCustomizeSocket(USkeletalMeshSocket* InSocketToCustomize)
{
	if (SkeletalMesh)
	{
		const FScopedTransaction Transaction(LOCTEXT("CreateMeshSocket", "Create Mesh Socket"));
		SkeletalMesh->Modify();

		USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(SkeletalMesh);
		check(NewSocket);

		NewSocket->BoneName = InSocketToCustomize->BoneName;
		NewSocket->SocketName = InSocketToCustomize->SocketName;
		NewSocket->RelativeLocation = InSocketToCustomize->RelativeLocation;
		NewSocket->RelativeRotation = InSocketToCustomize->RelativeRotation;
		NewSocket->RelativeScale = InSocketToCustomize->RelativeScale;

		SkeletalMesh->GetMeshOnlySocketList().Add(NewSocket);
	}
}

void FEditableSkeleton::HandlePromoteSocket(USkeletalMeshSocket* InSocketToPromote)
{
	const FScopedTransaction Transaction(LOCTEXT("PromoteSocket", "Promote Socket"));
	Skeleton->Modify();

	USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(Skeleton);
	check(NewSocket);

	NewSocket->BoneName = InSocketToPromote->BoneName;
	NewSocket->SocketName = InSocketToPromote->SocketName;
	NewSocket->RelativeLocation = InSocketToPromote->RelativeLocation;
	NewSocket->RelativeRotation = InSocketToPromote->RelativeRotation;
	NewSocket->RelativeScale = InSocketToPromote->RelativeScale;

	Skeleton->Sockets.Add(NewSocket);
}

void FEditableSkeleton::HandleRemoveAllAssets(TSharedPtr<IPersonaPreviewScene> InPreviewScene)
{
	FScopedTransaction Transaction(LOCTEXT("AttachedAssetRemoveUndo", "Remove All Attached Assets"));
	Skeleton->Modify();

	DeleteAttachedObjects(Skeleton->PreviewAttachedAssetContainer, InPreviewScene);

	if (SkeletalMesh)
	{
		SkeletalMesh->Modify();
		DeleteAttachedObjects(SkeletalMesh->PreviewAttachedAssetContainer, InPreviewScene);
	}
}

void FEditableSkeleton::DeleteAttachedObjects(FPreviewAssetAttachContainer& AttachedAssets, TSharedPtr<IPersonaPreviewScene> InPreviewScene)
{
	if (InPreviewScene.IsValid())
	{
		for (auto Iter = AttachedAssets.CreateIterator(); Iter; ++Iter)
		{
			FPreviewAttachedObjectPair& Pair = (*Iter);
			InPreviewScene->RemoveAttachedObjectFromPreviewComponent(Pair.GetAttachedObject(), Pair.AttachedTo);
		}
	}

	AttachedAssets.ClearAllAttachedObjects();
}

USkeletalMeshSocket* FEditableSkeleton::DuplicateSocket(const FSelectedSocketInfo& SocketInfoToDuplicate, const FName& NewParentBoneName)
{
	check(SocketInfoToDuplicate.Socket);

	const FScopedTransaction Transaction(LOCTEXT("CopySocket", "Copy Socket"));

	USkeletalMeshSocket* NewSocket;

	bool bModifiedSkeleton = false;

	if (SocketInfoToDuplicate.bSocketIsOnSkeleton)
	{
		Skeleton->Modify();
		bModifiedSkeleton = true;
		NewSocket = NewObject<USkeletalMeshSocket>(Skeleton);
	}
	else if (SkeletalMesh)
	{
		SkeletalMesh->Modify();
		NewSocket = NewObject<USkeletalMeshSocket>(SkeletalMesh);
	}
	else
	{
		// Original socket was on the mesh, but we have no mesh. Huh?
		check(0);
		return nullptr;
	}

	NewSocket->SocketName = GenerateUniqueSocketName(SocketInfoToDuplicate.Socket->SocketName);
	NewSocket->BoneName = NewParentBoneName != "" ? NewParentBoneName : SocketInfoToDuplicate.Socket->BoneName;
	NewSocket->RelativeLocation = SocketInfoToDuplicate.Socket->RelativeLocation;
	NewSocket->RelativeRotation = SocketInfoToDuplicate.Socket->RelativeRotation;
	NewSocket->RelativeScale = SocketInfoToDuplicate.Socket->RelativeScale;

	if (SocketInfoToDuplicate.bSocketIsOnSkeleton)
	{
		Skeleton->Sockets.Add(NewSocket);
	}
	else if (SkeletalMesh)
	{
		SkeletalMesh->GetMeshOnlySocketList().Add(NewSocket);
	}

	// Duplicated attached assets
	int32 NumExistingAttachedObjects = Skeleton->PreviewAttachedAssetContainer.Num();
	for (int AttachedObjectIndex = 0; AttachedObjectIndex < NumExistingAttachedObjects; ++AttachedObjectIndex)
	{
		FPreviewAttachedObjectPair& Pair = Skeleton->PreviewAttachedAssetContainer[AttachedObjectIndex];
		if (Pair.AttachedTo == SocketInfoToDuplicate.Socket->SocketName)
		{
			if (!bModifiedSkeleton)
			{
				bModifiedSkeleton = true;
				Skeleton->Modify();
			}

			for (TWeakPtr<SSkeletonTree> SkeletonTree : SkeletonTrees)
			{
				if (SkeletonTree.IsValid())
				{
					SkeletonTree.Pin()->PostDuplicateSocket(Pair.GetAttachedObject(), NewSocket->SocketName);
				}
			}

			// should be safe to call this even though we are growing the PreviewAttachedAssetContainer array as we cache the array count
			Skeleton->PreviewAttachedAssetContainer.AddUniqueAttachedObject(Pair.GetAttachedObject(), NewSocket->SocketName);
		}
	}

	OnTreeRefresh.Broadcast();

	return NewSocket;
}

int32 FEditableSkeleton::ValidatePreviewAttachedObjects()
{
	return Skeleton->ValidatePreviewAttachedObjects();
}

void FEditableSkeleton::RecreateBoneTree(USkeletalMesh* NewPreviewMesh)
{
	Skeleton->RecreateBoneTree(NewPreviewMesh);
}

void FEditableSkeleton::HandleAttachAssets(const TArray<UObject*>& InObjects, const FName& InAttachToName, bool bAttachToMesh, TSharedPtr<IPersonaPreviewScene> InPreviewScene)
{
	for (auto Iter = InObjects.CreateConstIterator(); Iter; ++Iter)
	{
		UObject* Object = (*Iter);

		if (bAttachToMesh)
		{
			if (SkeletalMesh != nullptr)
			{
				FScopedTransaction Transaction(LOCTEXT("DragDropAttachMeshUndo", "Attach Assets to Mesh"));
				SkeletalMesh->Modify();
				if (InPreviewScene.IsValid())
				{
					InPreviewScene->AttachObjectToPreviewComponent(Object, InAttachToName);
				}
				SkeletalMesh->PreviewAttachedAssetContainer.AddUniqueAttachedObject(Object, InAttachToName);
			}
		}
		else
		{
			FScopedTransaction Transaction(LOCTEXT("DragDropAttachSkeletonUndo", "Attach Assets to Skeleton"));
			Skeleton->Modify();
			if (InPreviewScene.IsValid())
			{
				InPreviewScene->AttachObjectToPreviewComponent(Object, InAttachToName);
			}
			Skeleton->PreviewAttachedAssetContainer.AddUniqueAttachedObject(Object, InAttachToName);
		}
	}
}

void FEditableSkeleton::SetBoneTranslationRetargetingModeRecursive(const TArray<FName>& InBoneNames, EBoneTranslationRetargetingMode::Type NewRetargetingMode)
{
	if (InBoneNames.Num())
	{
		const FScopedTransaction Transaction(LOCTEXT("SetBoneTranslationRetargetingModeRecursive", "Set Bone Translation Retargeting Mode Recursive"));
		Skeleton->Modify();

		for (const FName& BoneName : InBoneNames)
		{
			int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
			Skeleton->SetBoneTranslationRetargetingMode(BoneIndex, NewRetargetingMode, true);
		}

		FAssetNotifications::SkeletonNeedsToBeSaved(Skeleton);
	}
}

void FEditableSkeleton::HandleDeleteAttachedAssets(const TArray<FPreviewAttachedObjectPair>& InAttachedObjects, TSharedPtr<IPersonaPreviewScene> InPreviewScene)
{
	if (InAttachedObjects.Num() > 0)
	{
		Skeleton->Modify();
		if (SkeletalMesh != nullptr)
		{
			SkeletalMesh->Modify();
		}

		for (const FPreviewAttachedObjectPair& AttachedObject : InAttachedObjects)
		{
			Skeleton->PreviewAttachedAssetContainer.RemoveAttachedObject(AttachedObject.GetAttachedObject(), AttachedObject.AttachedTo);

			if (SkeletalMesh != nullptr)
			{
				SkeletalMesh->PreviewAttachedAssetContainer.RemoveAttachedObject(AttachedObject.GetAttachedObject(), AttachedObject.AttachedTo);

				if (InPreviewScene.IsValid())
				{
					InPreviewScene->RemoveAttachedObjectFromPreviewComponent(AttachedObject.GetAttachedObject(), AttachedObject.AttachedTo);
				}
			}
		}
	}
}

void FEditableSkeleton::HandleDeleteSockets(const TArray<FSelectedSocketInfo>& InSocketInfo, TSharedPtr<IPersonaPreviewScene> InPreviewScene)
{
	for (const FSelectedSocketInfo& SocketInfo : InSocketInfo)
	{
		FName SocketName = SocketInfo.Socket->SocketName;

		if (SocketInfo.bSocketIsOnSkeleton)
		{
			Skeleton->Modify();
			Skeleton->Sockets.Remove(SocketInfo.Socket);
		}
		else
		{
			if (SkeletalMesh != nullptr)
			{
				UObject* Object = SkeletalMesh->PreviewAttachedAssetContainer.GetAttachedObjectByAttachName(SocketName);
				if (Object)
				{
					SkeletalMesh->Modify();
					SkeletalMesh->PreviewAttachedAssetContainer.RemoveAttachedObject(Object, SocketName);
					if (InPreviewScene.IsValid())
					{
						InPreviewScene->RemoveAttachedObjectFromPreviewComponent(Object, SocketName);
					}
				}

				SkeletalMesh->Modify();
				SkeletalMesh->GetMeshOnlySocketList().Remove(SocketInfo.Socket);
			}
		}

		// Remove attached assets
		while (UObject* Object = Skeleton->PreviewAttachedAssetContainer.GetAttachedObjectByAttachName(SocketName))
		{
			Skeleton->Modify();
			Skeleton->PreviewAttachedAssetContainer.RemoveAttachedObject(Object, SocketName);
			if (InPreviewScene.IsValid())
			{
				InPreviewScene->RemoveAttachedObjectFromPreviewComponent(Object, SocketName);
			}
		}
	}

	OnTreeRefresh.Broadcast();
}

void FEditableSkeleton::RecursiveSetBlendProfileScales(const FName& InBlendProfileName, const TArray<FName>& InBoneNames, float InScaleToSet)
{
	const FScopedTransaction Transaction(LOCTEXT("SetBlendScalesRecursive", "Recursively Set Blend Profile Scales"));
	Skeleton->Modify();

	for (const FName& BoneName : InBoneNames)
	{
		SetBlendProfileScale(InBlendProfileName, BoneName, InScaleToSet, true);
	}

	FAssetNotifications::SkeletonNeedsToBeSaved(Skeleton);
}

TSharedRef<ISkeletonTree> FEditableSkeleton::CreateSkeletonTree(const FSkeletonTreeArgs& InSkeletonTreeArgs)
{
	// first compact widget tracking array
	SkeletonTrees.RemoveAll([](TWeakPtr<SSkeletonTree> InSkeletonTree) { return !InSkeletonTree.IsValid(); });

	// build new tree
	TSharedRef<SSkeletonTree> SkeletonTree = SNew(SSkeletonTree, AsShared(), InSkeletonTreeArgs);

	OnTreeRefresh.AddSP(&SkeletonTree.Get(), &SSkeletonTree::HandleTreeRefresh);

	SkeletonTrees.Add(SkeletonTree);
	return SkeletonTree;
}

TSharedRef<SWidget> FEditableSkeleton::CreateBlendProfilePicker(const FBlendProfilePickerArgs& InArgs)
{
	// first compact widget tracking array
	BlendProfilePickers.RemoveAll([](TWeakPtr<SBlendProfilePicker> InBlendProfilePicker) { return !InBlendProfilePicker.IsValid(); });

	// build new picker
	TSharedRef<SBlendProfilePicker> BlendProfilePicker = SNew(SBlendProfilePicker, AsShared())
		.InitialProfile(InArgs.InitialProfile)
		.OnBlendProfileSelected(InArgs.OnBlendProfileSelected)
		.AllowNew(InArgs.bAllowNew)
		.AllowClear(InArgs.bAllowClear);

	BlendProfilePickers.Add(BlendProfilePicker);
	return BlendProfilePicker;
}

int32 FEditableSkeleton::DeleteAnimNotifies(const TArray<FName>& InNotifyNames)
{
	const FScopedTransaction Transaction(LOCTEXT("DeleteAnimNotify", "Delete Anim Notify"));
	Skeleton->Modify();

	TArray<FAssetData> CompatibleAnimSequences;
	GetCompatibleAnimSequences(CompatibleAnimSequences);

	int32 NumAnimationsModified = 0;

	for (int32 AssetIndex = 0; AssetIndex < CompatibleAnimSequences.Num(); ++AssetIndex)
	{
		const FAssetData& PossibleAnimSequence = CompatibleAnimSequences[AssetIndex];
		UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PossibleAnimSequence.GetAsset());

		bool SequenceModified = false;
		for (int32 NotifyIndex = Sequence->Notifies.Num() - 1; NotifyIndex >= 0; --NotifyIndex)
		{
			FAnimNotifyEvent& AnimNotify = Sequence->Notifies[NotifyIndex];
			if (InNotifyNames.Contains(AnimNotify.NotifyName))
			{
				if (!SequenceModified)
				{
					Sequence->Modify();
					++NumAnimationsModified;
					SequenceModified = true;
				}
				Sequence->Notifies.RemoveAtSwap(NotifyIndex);
			}
		}

		if (SequenceModified)
		{
			Sequence->MarkPackageDirty();
		}
	}

	FBlueprintActionDatabase::Get().RefreshAssetActions(Skeleton);

	return NumAnimationsModified;
}

int32 FEditableSkeleton::RenameNotify(const FName& NewName, const FName& OldName)
{
	const FScopedTransaction Transaction(LOCTEXT("RenameAnimNotify", "Rename Anim Notify"));
	Skeleton->Modify();

	int32 Index = Skeleton->AnimationNotifies.IndexOfByKey(OldName);
	Skeleton->AnimationNotifies[Index] = NewName;

	TArray<FAssetData> CompatibleAnimSequences;
	GetCompatibleAnimSequences(CompatibleAnimSequences);

	int32 NumAnimationsModified = 0;

	for (int32 AssetIndex = 0; AssetIndex < CompatibleAnimSequences.Num(); ++AssetIndex)
	{
		const FAssetData& PossibleAnimSequence = CompatibleAnimSequences[AssetIndex];
		UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(PossibleAnimSequence.GetAsset());

		bool SequenceModified = false;
		for (int32 NotifyIndex = Sequence->Notifies.Num() - 1; NotifyIndex >= 0; --NotifyIndex)
		{
			FAnimNotifyEvent& AnimNotify = Sequence->Notifies[NotifyIndex];
			if (OldName == AnimNotify.NotifyName)
			{
				if (!SequenceModified)
				{
					Sequence->Modify();
					++NumAnimationsModified;
					SequenceModified = true;
				}
				AnimNotify.NotifyName = NewName;
			}
		}

		if (SequenceModified)
		{
			Sequence->MarkPackageDirty();
		}
	}

	return NumAnimationsModified;
}

void FEditableSkeleton::GetCompatibleAnimSequences(TArray<class FAssetData>& OutAssets)
{
	//Get the skeleton tag to search for
	FString SkeletonExportName = FAssetData(Skeleton).GetExportTextName();

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetFName(), AssetDataList, true);

	OutAssets.Empty(AssetDataList.Num());

	for (int32 AssetIndex = 0; AssetIndex < AssetDataList.Num(); ++AssetIndex)
	{
		const FAssetData& PossibleAnimSequence = AssetDataList[AssetIndex];
		if (SkeletonExportName == PossibleAnimSequence.GetTagValueRef<FString>("Skeleton"))
		{
			OutAssets.Add(PossibleAnimSequence);
		}
	}
}

void FEditableSkeleton::RegisterOnSkeletonHierarchyChanged(const USkeleton::FOnSkeletonHierarchyChanged& InDelegate)
{
	Skeleton->RegisterOnSkeletonHierarchyChanged(InDelegate);
}

void FEditableSkeleton::UnregisterOnSkeletonHierarchyChanged(void* Thing)
{
	if (Skeleton)
	{
		Skeleton->UnregisterOnSkeletonHierarchyChanged(Thing);
	}
}

void FEditableSkeleton::SetPreviewMesh(class USkeletalMesh* InSkeletalMesh)
{
	const FScopedTransaction Transaction(LOCTEXT("ChangeSkeletonPreviewMesh", "Change Skeleton Preview Mesh"));
	Skeleton->SetPreviewMesh(InSkeletalMesh);
}

void FEditableSkeleton::LoadAdditionalPreviewSkeletalMeshes()
{
	Skeleton->LoadAdditionalPreviewSkeletalMeshes();
}

void FEditableSkeleton::SetAdditionalPreviewSkeletalMeshes(class UPreviewMeshCollection* InSkeletalMeshes)
{
	const FScopedTransaction Transaction(LOCTEXT("ChangeSkeletonAdditionalMeshes", "Change Skeleton Additional Meshes"));
	Skeleton->SetAdditionalPreviewSkeletalMeshes(InSkeletalMeshes);
}

void FEditableSkeleton::RenameRetargetSource(const FName& InOldName, const FName& InNewName)
{
	const FScopedTransaction Transaction(LOCTEXT("RetargetSourceWindow_Rename", "Rename Retarget Source"));

	FReferencePose* Pose = Skeleton->AnimRetargetSources.Find(InOldName);
	if (Pose)
	{
		FReferencePose NewPose = *Pose;
		NewPose.PoseName = InNewName;

		Skeleton->Modify();

		Skeleton->AnimRetargetSources.Remove(InOldName);
		Skeleton->AnimRetargetSources.Add(InNewName, NewPose);

		// need to verify if these pose is used by anybody else
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		TArray<FAssetData> AssetList;
		TMultiMap<FName, FString> TagsAndValues;
		TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UAnimSequence, RetargetSource), InOldName.ToString());
		AssetRegistryModule.Get().GetAssetsByTagValues(TagsAndValues, AssetList);

		// ask users if they'd like to continue and/or fix up
		if (AssetList.Num() > 0)
		{
			FString ListOfAssets;
			// if users is sure to delete, delete
			for (auto Iter = AssetList.CreateConstIterator(); Iter; ++Iter)
			{
				ListOfAssets += Iter->AssetName.ToString();
				ListOfAssets += TEXT("\n");
			}


			// if so, ask if they'd like us to fix the animations as well
			FString Message = NSLOCTEXT("RetargetSourceEditor", "RetargetSourceRename_FixUpAnimation_Message", "Would you like to fix up the following animation(s)? You'll have to save all the assets in the list.").ToString();
			Message += TEXT("\n");
			Message += ListOfAssets;

			EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Message));

			if (Result == EAppReturnType::Yes)
			{
				// now fix up all assets
				TArray<UObject*> ObjectsToUpdate;
				for (auto Iter = AssetList.CreateConstIterator(); Iter; ++Iter)
				{
					UAnimSequence * AnimSequence = Cast<UAnimSequence>(Iter->GetAsset());
					if (AnimSequence)
					{
						ObjectsToUpdate.Add(AnimSequence);

						AnimSequence->Modify();
						// clear name
						AnimSequence->RetargetSource = InNewName;
					}
				}
			}
		}

		Skeleton->CallbackRetargetSourceChanged();
	}
}

void FEditableSkeleton::AddRetargetSource(const FName& InName, USkeletalMesh* InReferenceMesh)
{
	// need to verify if the name is unique, if not create unique name
	int32 IntSuffix = 1;
	FReferencePose * ExistingPose = NULL;
	FString NewSourceName = TEXT("");
	do
	{
		if (IntSuffix <= 1)
		{
			NewSourceName = FString::Printf(TEXT("%s"), *InName.ToString());
		}
		else
		{
			NewSourceName = FString::Printf(TEXT("%s%d"), *InName.ToString(), IntSuffix);
		}

		ExistingPose = Skeleton->AnimRetargetSources.Find(FName(*NewSourceName));
		IntSuffix++;
	} while (ExistingPose != NULL);

	// add new one
	// remap to skeleton refpose
	// we have to do this whenever skeleton changes
	FReferencePose RefPose;
	RefPose.PoseName = FName(*NewSourceName);
	RefPose.ReferenceMesh = InReferenceMesh;

	const FScopedTransaction Transaction(LOCTEXT("RetargetSourceWindow_Add", "Add New Retarget Source"));
	Skeleton->Modify();

	Skeleton->AnimRetargetSources.Add(*NewSourceName, RefPose);
	// ask skeleton to update retarget source for the given name
	Skeleton->UpdateRetargetSource(*NewSourceName);

	Skeleton->CallbackRetargetSourceChanged();
}


void FEditableSkeleton::DeleteRetargetSources(const TArray<FName>& InRetargetSourceNames)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	const FScopedTransaction Transaction(LOCTEXT("RetargetSourceWindow_Delete", "Delete Retarget Source"));
	for (const FName& SourceName : InRetargetSourceNames)
	{
		const FReferencePose* PoseFound = Skeleton->AnimRetargetSources.Find(SourceName);
		if (PoseFound)
		{
			// need to verify if these pose is used by anybody else
			TArray<FAssetData> AssetList;
			TMultiMap<FName, FString> TagsAndValues;
			TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UAnimSequence, RetargetSource), PoseFound->PoseName.ToString());
			AssetRegistryModule.Get().GetAssetsByTagValues(TagsAndValues, AssetList);

			// ask users if they'd like to continue and/or fix up
			if (AssetList.Num() > 0)
			{
				FString ListOfAssets;
				// if users is sure to delete, delete
				for (auto Iter = AssetList.CreateConstIterator(); Iter; ++Iter)
				{
					ListOfAssets += Iter->AssetName.ToString();
					ListOfAssets += TEXT("\n");
				}

				// ask if they'd like to continue deleting this pose regardless animation references
				FString Message = NSLOCTEXT("RetargetSourceEditor", "RetargetSourceDeleteMessage", "Following animation(s) is(are) referencing this pose. Are you sure if you'd like to delete this pose?").ToString();
				Message += TEXT("\n\n");
				Message += ListOfAssets;

				EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Message));

				if (Result == EAppReturnType::No)
				{
					continue;
				}

				// if so, ask if they'd like us to fix the animations as well
				Message = NSLOCTEXT("RetargetSourceEditor", "RetargetSourceDelete_FixUpAnimation_Message", "Would you like to fix up the following animation(s)? You'll have to save all the assets in the list.").ToString();
				Message += TEXT("\n");
				Message += ListOfAssets;

				Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Message));

				if (Result == EAppReturnType::No)
				{
					continue;
				}

				// now fix up all assets
				TArray<UObject *> ObjectsToUpdate;
				for (auto Iter = AssetList.CreateConstIterator(); Iter; ++Iter)
				{
					UAnimSequence * AnimSequence = Cast<UAnimSequence>(Iter->GetAsset());
					if (AnimSequence)
					{
						ObjectsToUpdate.Add(AnimSequence);

						AnimSequence->Modify();
						// clear name
						AnimSequence->RetargetSource = NAME_None;
					}
				}
			}

			Skeleton->Modify();
			// delete now
			Skeleton->AnimRetargetSources.Remove(SourceName);
			Skeleton->CallbackRetargetSourceChanged();
		}
	}
}

void FEditableSkeleton::RefreshRetargetSources(const TArray<FName>& InRetargetSourceNames)
{
	const FScopedTransaction Transaction(LOCTEXT("RetargetSourceWindow_Refresh", "Refresh Retarget Source"));
	for (const FName& RetargetSourceName : InRetargetSourceNames)
	{
		Skeleton->Modify();

		// refresh pose now
		Skeleton->UpdateRetargetSource(RetargetSourceName);

		// feedback of pose has been updated
		FFormatNamedArguments Args;
		Args.Add(TEXT("RetargetSourceName"), FText::FromString(RetargetSourceName.ToString()));
		FNotificationInfo Info(FText::Format(LOCTEXT("RetargetSourceWindow_RefreshPose", "Retarget Source {RetargetSourceName} is refreshed"), Args));
		Info.ExpireDuration = 5.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			Notification->SetCompletionState(SNotificationItem::CS_None);
		}
	}
}

void FEditableSkeleton::RefreshRigConfig()
{
	Skeleton->RefreshRigConfig();
}

void FEditableSkeleton::SetRigConfig(URig* InRig)
{
	const FScopedTransaction Transaction(LOCTEXT("RigAssetChanged", "Select Rig"));
	Skeleton->Modify();
	Skeleton->SetRigConfig(InRig);
}

void FEditableSkeleton::SetRigBoneMapping(const FName& InNodeName, const FName& InBoneName)
{
	const FScopedTransaction Transaction(LOCTEXT("BoneMappingChanged", "Change Bone Mapping"));
	Skeleton->Modify();
	Skeleton->SetRigBoneMapping(InNodeName, InBoneName);
}

void FEditableSkeleton::SetRigBoneMappings(const TMap<FName, FName>& InMappings)
{
	const FScopedTransaction Transaction(LOCTEXT("BoneMappingsChanged", "Change Bone Mappings"));
	Skeleton->Modify();

	for (const TPair<FName, FName>& Mapping : InMappings)
	{
		Skeleton->SetRigBoneMapping(Mapping.Key, Mapping.Value);
	}
}

void FEditableSkeleton::RemoveUnusedBones()
{
	TArray<FName> SkeletonBones;
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		SkeletonBones.Add(RefSkeleton.GetBoneName(BoneIndex));
	}

	FARFilter Filter;
	Filter.ClassNames.Add(USkeletalMesh::StaticClass()->GetFName());

	FString SkeletonString = FAssetData(Skeleton).GetExportTextName();
	Filter.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(USkeletalMesh, Skeleton), SkeletonString);

	TArray<FAssetData> SkeletalMeshes;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().GetAssets(Filter, SkeletalMeshes);

	FText TimeTakenMessage = FText::Format(LOCTEXT("TimeTakenWarning", "In order to verify bone use all Skeletal Meshes that use this skeleton will be loaded, this may take some time.\n\nProceed?\n\nNumber of Meshes: {0}"), FText::AsNumber(SkeletalMeshes.Num()));

	if (FMessageDialog::Open(EAppMsgType::YesNo, TimeTakenMessage) == EAppReturnType::Yes)
	{
		const FText StatusUpdate = FText::Format(LOCTEXT("RemoveUnusedBones_ProcessingAssetsFor", "Processing Skeletal Meshes for {0}"), FText::FromString(Skeleton->GetName()));
		GWarn->BeginSlowTask(StatusUpdate, true);

		// Loop through all SkeletalMeshes and remove the bones they use from our list
		for (int32 MeshIdx = 0; MeshIdx < SkeletalMeshes.Num(); ++MeshIdx)
		{
			GWarn->StatusUpdate(MeshIdx, SkeletalMeshes.Num(), StatusUpdate);

			USkeletalMesh* Mesh = Cast<USkeletalMesh>(SkeletalMeshes[MeshIdx].GetAsset());
			const FReferenceSkeleton& MeshRefSkeleton = Mesh->RefSkeleton;

			for (int32 BoneIndex = 0; BoneIndex < MeshRefSkeleton.GetNum(); ++BoneIndex)
			{
				SkeletonBones.Remove(MeshRefSkeleton.GetBoneName(BoneIndex));
				if (SkeletonBones.Num() == 0)
				{
					break;
				}
			}
			if (SkeletonBones.Num() == 0)
			{
				break;
			}
		}

		GWarn->EndSlowTask();

		//Remove bones that are a parent to bones we aren't removing
		for (int32 BoneIndex = RefSkeleton.GetNum() - 1; BoneIndex >= 0; --BoneIndex)
		{
			FName CurrBoneName = RefSkeleton.GetBoneName(BoneIndex);
			if (!SkeletonBones.Contains(CurrBoneName))
			{
				//We aren't removing this bone, so remove parent from list of bones to remove too
				int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
				if (ParentIndex != INDEX_NONE)
				{
					SkeletonBones.Remove(RefSkeleton.GetBoneName(ParentIndex));
				}
			}
		}

		// If we have any bones left they are unused
		if (SkeletonBones.Num() > 0)
		{
			const FText RemoveBoneMessage = FText::Format(LOCTEXT("RemoveBoneWarning", "Continuing will remove the following bones from the skeleton '{0}'. These bones are not being used by any of the SkeletalMeshes assigned to this skeleton\n\nOnce the bones have been removed all loaded animations for this skeleton will be recompressed (any that aren't loaded will be recompressed the next time they are loaded)."), FText::FromString(Skeleton->GetName()));

			// Ask User whether they would like to remove the bones from the skeleton
			if (SSkeletonBoneRemoval::ShowModal(SkeletonBones, RemoveBoneMessage))
			{
				//Remove these bones from the skeleton
				Skeleton->RemoveBonesFromSkeleton(SkeletonBones, true);
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoBonesToRemove", "No unused bones were found."));
		}
	}

	OnTreeRefresh.Broadcast();
}

void FEditableSkeleton::UpdateSkeletonReferencePose(USkeletalMesh* InSkeletalMesh)
{
	Skeleton->UpdateReferencePoseFromMesh(InSkeletalMesh);
}

bool FEditableSkeleton::AddSlotGroupName(const FName& InSlotName)
{
	const FScopedTransaction Transaction(LOCTEXT("AddSlotGroupName", "Add Slot Group Name"));
	Skeleton->Modify();
	return Skeleton->AddSlotGroupName(InSlotName);
}

void FEditableSkeleton::SetSlotGroupName(const FName& InSlotName, const FName& InGroupName)
{
	const FScopedTransaction Transaction(LOCTEXT("SetSlotGroupName", "Set Slot Group Name"));
	Skeleton->Modify();
	Skeleton->SetSlotGroupName(InSlotName, InGroupName);
}

void FEditableSkeleton::DeleteSlotName(const FName& InSlotName)
{
	const FScopedTransaction Transaction(LOCTEXT("DeleteSlotName", "Delete Slot Name"));
	Skeleton->Modify();
	Skeleton->RemoveSlotName(InSlotName);
}

void FEditableSkeleton::DeleteSlotGroup(const FName& InGroupName)
{
	const FScopedTransaction Transaction(LOCTEXT("DeleteSlotGroup", "Delete Slot Group"));
	Skeleton->Modify();
	Skeleton->RemoveSlotGroup(InGroupName);
}

void FEditableSkeleton::RenameSlotName(const FName& InOldSlotName, const FName& InNewSlotName)
{
	const FScopedTransaction Transaction(LOCTEXT("RenameSlotName", "Rename Slot Name"));
	Skeleton->Modify();
	Skeleton->RenameSlotName(InOldSlotName, InNewSlotName);
}

FDelegateHandle FEditableSkeleton::RegisterOnSmartNameRemoved(const FOnSmartNameRemoved::FDelegate& InOnSmartNameRemoved)
{
	return OnSmartNameRemoved.Add(InOnSmartNameRemoved);
}

void FEditableSkeleton::UnregisterOnSmartNameRemoved(FDelegateHandle InHandle)
{
	OnSmartNameRemoved.Remove(InHandle);
}

#undef LOCTEXT_NAMESPACE