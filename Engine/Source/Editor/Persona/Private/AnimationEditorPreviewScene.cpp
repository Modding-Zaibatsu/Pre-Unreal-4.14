// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"

#include "AnimationEditorPreviewScene.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "NotificationManager.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "IPersonaToolkit.h"
#include "PersonaUtils.h"
#include "ComponentAssetBroker.h"
#include "IEditableSkeleton.h"
#include "Engine/PreviewMeshCollection.h"
#include "PersonaPreviewSceneDescription.h"
#include "Engine/WindDirectionalSource.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"

#define LOCTEXT_NAMESPACE "AnimationEditorPreviewScene"

/////////////////////////////////////////////////////////////////////////
// FAnimationEditorPreviewScene

FAnimationEditorPreviewScene::FAnimationEditorPreviewScene(const ConstructionValues& CVS, const TSharedRef<IEditableSkeleton>& InEditableSkeleton, const TSharedRef<IPersonaToolkit>& InPersonaToolkit)
	: IPersonaPreviewScene(CVS)
	, EditableSkeletonPtr(InEditableSkeleton)
	, PersonaToolkit(InPersonaToolkit)
	, DefaultMode(EPreviewSceneDefaultAnimationMode::ReferencePose)
	, PrevWindLocation(100.0f, 100.0f, 100.0f)
	, PrevWindRotation(0.0f, 0.0f, 0.0f)
	, PrevWindStrength(0.2f)
	, GravityScale(0.25f)
	, SelectedBoneIndex(INDEX_NONE)
{
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
	
	FloorBounds = FloorMeshComponent->CalcBounds(FloorMeshComponent->GetRelativeTransform());

	// Create the preview component
	SkeletalMeshComponent = NewObject<UDebugSkelMeshComponent>(GetTransientPackage());
	SkeletalMeshComponent->AddToRoot();

	InEditableSkeleton->LoadAdditionalPreviewSkeletalMeshes();

	// create the preview scene description
	PreviewSceneDescription = NewObject<UPersonaPreviewSceneDescription>(GetTransientPackage());
	PreviewSceneDescription->AddToRoot();

	PreviewSceneDescription->AnimationMode = EPreviewAnimationMode::Default;
	PreviewSceneDescription->Animation = InPersonaToolkit->GetAnimationAsset();
	PreviewSceneDescription->PreviewMesh = InPersonaToolkit->GetSkeleton()->GetPreviewMesh();
	PreviewSceneDescription->AdditionalMeshes = InEditableSkeleton->GetSkeleton().GetAdditionalPreviewSkeletalMeshes();

	// Force validation of preview attached assets (catch case of never doing it if we dont have a valid preview mesh)
	ValidatePreviewAttachedAssets(nullptr);

	RefreshAdditionalMeshes();
}

FAnimationEditorPreviewScene::~FAnimationEditorPreviewScene()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}

	SkeletalMeshComponent->RemoveFromRoot();
	PreviewSceneDescription->RemoveFromRoot();
}

void FAnimationEditorPreviewScene::SetPreviewMesh(USkeletalMesh* NewPreviewMesh)
{
	const USkeleton& Skeleton = GetEditableSkeleton()->GetSkeleton();

	if (NewPreviewMesh != nullptr && !Skeleton.IsCompatibleMesh(NewPreviewMesh))
	{
		// message box, ask if they'd like to regenerate skeleton
		if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("RenerateSkeleton", "The preview mesh hierarchy doesn't match with Skeleton anymore. Would you like to regenerate skeleton?")) == EAppReturnType::Yes)
		{
			GetEditableSkeleton()->RecreateBoneTree(NewPreviewMesh);
			SetPreviewMeshInternal(NewPreviewMesh);
		}
		else
		{
			// Send a notification that the skeletal mesh cannot work with the skeleton
			FFormatNamedArguments Args;
			Args.Add(TEXT("PreviewMeshName"), FText::FromString(NewPreviewMesh->GetName()));
			Args.Add(TEXT("TargetSkeletonName"), FText::FromString(Skeleton.GetName()));
			FNotificationInfo Info(FText::Format(LOCTEXT("SkeletalMeshIncompatible", "Skeletal Mesh \"{PreviewMeshName}\" incompatible with Skeleton \"{TargetSkeletonName}\""), Args));
			Info.ExpireDuration = 3.0f;
			Info.bUseLargeFont = false;
			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if (Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Fail);
			}
		}
	}
	else
	{
		SetPreviewMeshInternal(NewPreviewMesh);
	}

	// changing the main skeletal mesh may mean re-applying the additional meshes
	// as the mesh on the main component may have been substituted by one of the additional meshes
	RefreshAdditionalMeshes();
}

void FAnimationEditorPreviewScene::SetPreviewMeshInternal(USkeletalMesh* NewPreviewMesh)
{
	ValidatePreviewAttachedAssets(NewPreviewMesh);

	if (NewPreviewMesh != SkeletalMeshComponent->SkeletalMesh)
	{
		// setting skeletalmesh unregister/re-register, 
		// so I have to save the animation settings and resetting after setting mesh
		UAnimationAsset* AnimAssetToPlay = nullptr;
		float PlayPosition = 0.f;
		bool bPlaying = false;
		bool bNeedsToCopyAnimationData = SkeletalMeshComponent->GetAnimInstance() && SkeletalMeshComponent->GetAnimInstance() == SkeletalMeshComponent->PreviewInstance;
		if (bNeedsToCopyAnimationData)
		{
			AnimAssetToPlay = SkeletalMeshComponent->PreviewInstance->GetCurrentAsset();
			PlayPosition = SkeletalMeshComponent->PreviewInstance->GetCurrentTime();
			bPlaying = SkeletalMeshComponent->PreviewInstance->IsPlaying();
		}

		SkeletalMeshComponent->SetSkeletalMesh(NewPreviewMesh);

		if (bNeedsToCopyAnimationData)
		{
			SetPreviewAnimationAsset(AnimAssetToPlay);
			SkeletalMeshComponent->PreviewInstance->SetPosition(PlayPosition);
			SkeletalMeshComponent->PreviewInstance->SetPlaying(bPlaying);
		}
	}
	else
	{
		SkeletalMeshComponent->InitAnim(true);
	}

	if (NewPreviewMesh != nullptr)
	{
		AddComponent(SkeletalMeshComponent, FTransform::Identity);
		for (auto Iter = AdditionalMeshes.CreateIterator(); Iter; ++Iter)
		{
			AddComponent((*Iter), FTransform::Identity);
		}

		// Set up the mesh for transactions
		NewPreviewMesh->SetFlags(RF_Transactional);

		AddPreviewAttachedObjects();

		SkeletalMeshComponent->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
		SkeletalMeshComponent->bCanHighlightSelectedSections = true;
	}

	for (auto Iter = AdditionalMeshes.CreateIterator(); Iter; ++Iter)
	{
		(*Iter)->SetMasterPoseComponent(SkeletalMeshComponent);
		(*Iter)->UpdateMasterBoneMap();
	}

	// Setting the skeletal mesh to in the PreviewScene can change AnimScriptInstance so we must re register it
	// with the AnimBlueprint
	if (SkeletalMeshComponent->IsAnimBlueprintInstanced())
	{
		UAnimBlueprint* SourceBlueprint = CastChecked<UAnimBlueprint>(SkeletalMeshComponent->GetAnimInstance()->GetClass()->ClassGeneratedBy);
		SourceBlueprint->SetObjectBeingDebugged(SkeletalMeshComponent->GetAnimInstance());
	}

	OnPreviewMeshChanged.Broadcast(NewPreviewMesh);

	// make sure the scene desc is up to date, as this funciton is not necessarily called from the UI code that customizes it
	PreviewSceneDescription->PreviewMesh = NewPreviewMesh;
}

void FAnimationEditorPreviewScene::ValidatePreviewAttachedAssets(USkeletalMesh* PreviewSkeletalMesh)
{
	// Validate the skeleton/meshes attached objects and display a notification to the user if any were broken
	int32 NumBrokenAssets = GetEditableSkeleton()->ValidatePreviewAttachedObjects();
	if (PreviewSkeletalMesh)
	{
		NumBrokenAssets += PreviewSkeletalMesh->ValidatePreviewAttachedObjects();
	}

	if (NumBrokenAssets > 0)
	{
		// Tell the user that there were assets that could not be loaded
		FFormatNamedArguments Args;
		Args.Add(TEXT("NumBrokenAssets"), NumBrokenAssets);
		FNotificationInfo Info(FText::Format(LOCTEXT("MissingPreviewAttachedAssets", "{NumBrokenAssets} attached assets could not be found on loading and were removed"), Args));

		Info.bUseLargeFont = false;
		Info.ExpireDuration = 5.0f;

		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			Notification->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}
}

void FAnimationEditorPreviewScene::SetAdditionalMeshes(class UPreviewMeshCollection* InAdditionalMeshes)
{
	GetEditableSkeleton()->SetAdditionalPreviewSkeletalMeshes(InAdditionalMeshes);

	RefreshAdditionalMeshes();
}

void FAnimationEditorPreviewScene::RefreshAdditionalMeshes()
{
	UPreviewMeshCollection* SkeletonAdditionalMeshes = GetEditableSkeleton()->GetSkeleton().GetAdditionalPreviewSkeletalMeshes();

	// remove all components
	for (USkeletalMeshComponent* Component : AdditionalMeshes)
	{
		RemoveComponent(Component);
	}

	AdditionalMeshes.Empty();

	// add new components
	if (SkeletonAdditionalMeshes != nullptr)
	{
		const int32 NumMeshes = SkeletonAdditionalMeshes->SkeletalMeshes.Num();
		if (NumMeshes > 0)
		{
			// if the master component has no skeletal mesh, we need to set up one of the additional meshes
			// as the master instead
			int32 StartIndex = 0;
			if (PreviewSceneDescription->PreviewMesh == nullptr)
			{
				SetPreviewMeshInternal(SkeletonAdditionalMeshes->SkeletalMeshes[0].SkeletalMesh.LoadSynchronous());
				StartIndex++;
			}

			for (int32 MeshIndex = StartIndex; MeshIndex < NumMeshes; ++MeshIndex)
			{
				FPreviewMeshCollectionEntry& Entry = SkeletonAdditionalMeshes->SkeletalMeshes[MeshIndex];
				USkeletalMesh* SkeletalMesh = Entry.SkeletalMesh.LoadSynchronous();
				if (SkeletalMesh)
				{
					USkeletalMeshComponent* NewComp = NewObject<USkeletalMeshComponent>();
					NewComp->SetMasterPoseComponent(SkeletalMeshComponent);
					NewComp->SetSkeletalMesh(SkeletalMesh);
					NewComp->UpdateMasterBoneMap();
					AddComponent(NewComp, FTransform::Identity);
					AdditionalMeshes.Add(NewComp);
				}
			}
		}
		else if (PreviewSceneDescription->PreviewMesh == nullptr)
		{
			SetPreviewMeshInternal(nullptr);
		}
	}
}

void FAnimationEditorPreviewScene::AddPreviewAttachedObjects()
{
	// Load up mesh attachments...
	USkeletalMesh* Mesh = PersonaToolkit.Pin()->GetMesh();

	if ( Mesh )
	{
		for(int32 i = 0; i < Mesh->PreviewAttachedAssetContainer.Num(); i++)
		{
			FPreviewAttachedObjectPair& PreviewAttachedObject = Mesh->PreviewAttachedAssetContainer[i];

			AttachObjectToPreviewComponent(PreviewAttachedObject.GetAttachedObject(), PreviewAttachedObject.AttachedTo);
		}
	}

	const USkeleton& Skeleton = GetEditableSkeleton()->GetSkeleton();

	// ...and then skeleton attachments
	for(int32 i = 0; i < Skeleton.PreviewAttachedAssetContainer.Num(); i++)
	{
		const FPreviewAttachedObjectPair& PreviewAttachedObject = Skeleton.PreviewAttachedAssetContainer[i];

		AttachObjectToPreviewComponent(PreviewAttachedObject.GetAttachedObject(), PreviewAttachedObject.AttachedTo);
	}
}

bool FAnimationEditorPreviewScene::AttachObjectToPreviewComponent( UObject* Object, FName AttachTo )
{
	if(PersonaUtils::GetComponentForAttachedObject(SkeletalMeshComponent, Object, AttachTo))
	{
		return false; // Already have this asset attached at this location
	}

	TSubclassOf<UActorComponent> ComponentClass = FComponentAssetBrokerage::GetPrimaryComponentForAsset(Object->GetClass());
	if(SkeletalMeshComponent && *ComponentClass && ComponentClass->IsChildOf(USceneComponent::StaticClass()))
	{
		//set up world info for undo
		AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings();
		WorldSettings->SetFlags(RF_Transactional);
		WorldSettings->Modify();

		USceneComponent* SceneComponent = NewObject<USceneComponent>(WorldSettings, ComponentClass, NAME_None, RF_Transactional);

		FComponentAssetBrokerage::AssignAssetToComponent(SceneComponent, Object);

		if(UParticleSystemComponent* NewPSysComp = Cast<UParticleSystemComponent>(SceneComponent))
		{
			//maybe this should happen in FComponentAssetBrokerage::AssignAssetToComponent?
			NewPSysComp->SetTickGroup(TG_PostUpdateWork);
		}

		//set up preview component for undo
		SkeletalMeshComponent->SetFlags(RF_Transactional);
		SkeletalMeshComponent->Modify();

		// Attach component to the preview component
		SceneComponent->SetupAttachment(SkeletalMeshComponent, AttachTo);
		SceneComponent->RegisterComponent();
		return true;
	}
	return false;
}

void FAnimationEditorPreviewScene::RemoveAttachedObjectFromPreviewComponent(UObject* Object, FName AttachedTo)
{
	// clean up components	
	if (SkeletalMeshComponent)
	{
		AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings();
		WorldSettings->SetFlags(RF_Transactional);
		WorldSettings->Modify();

		//set up preview component for undo
		SkeletalMeshComponent->SetFlags(RF_Transactional);
		SkeletalMeshComponent->Modify();

		for (int32 I= SkeletalMeshComponent->GetAttachChildren().Num()-1; I >= 0; --I) // Iterate backwards because CleanupComponent will remove from AttachChildren
		{
			USceneComponent* ChildComponent = SkeletalMeshComponent->GetAttachChildren()[I];
			UObject* Asset = FComponentAssetBrokerage::GetAssetFromComponent(ChildComponent);

			if( Asset == Object && ChildComponent->GetAttachSocketName() == AttachedTo)
			{
				// PreviewComponet will be cleaned up by PreviewScene, 
				// but if anything is attached, it won't be cleaned up, 
				// so we'll need to clean them up manually
				CleanupComponent(SkeletalMeshComponent->GetAttachChildren()[I]);
				break;
			}
		}
	}
}

void FAnimationEditorPreviewScene::InvalidateViews()
{
	OnInvalidateViews.Broadcast();
}

void FAnimationEditorPreviewScene::RemoveAttachedComponent( bool bRemovePreviewAttached /* = true */ )
{
	const USkeleton& Skeleton = GetEditableSkeleton()->GetSkeleton();

	TMap<UObject*, TArray<FName>> PreviewAttachedObjects;

	if( !bRemovePreviewAttached )
	{
		for(auto Iter = Skeleton.PreviewAttachedAssetContainer.CreateConstIterator(); Iter; ++Iter)
		{
			PreviewAttachedObjects.FindOrAdd(Iter->GetAttachedObject()).Add(Iter->AttachedTo);
		}

		if ( USkeletalMesh* PreviewMesh = PersonaToolkit.Pin()->GetMesh() )
		{
			for(auto Iter = PreviewMesh->PreviewAttachedAssetContainer.CreateConstIterator(); Iter; ++Iter)
			{
				PreviewAttachedObjects.FindOrAdd(Iter->GetAttachedObject()).Add(Iter->AttachedTo);
			}
		}
	}

	// clean up components	
	if (SkeletalMeshComponent)
	{
		for (int32 I= SkeletalMeshComponent->GetAttachChildren().Num()-1; I >= 0; --I) // Iterate backwards because CleanupComponent will remove from AttachChildren
		{
			USceneComponent* ChildComponent = SkeletalMeshComponent->GetAttachChildren()[I];
			UObject* Asset = FComponentAssetBrokerage::GetAssetFromComponent(ChildComponent);

			bool bRemove = true;

			//are we supposed to leave assets that came from the skeleton
			if(	!bRemovePreviewAttached )
			{
				//could this asset have come from the skeleton
				if(PreviewAttachedObjects.Contains(Asset))
				{
					if(PreviewAttachedObjects.Find(Asset)->Contains(ChildComponent->GetAttachSocketName()))
					{
						bRemove = false;
					}
				}
			}

			if(bRemove)
			{
				// PreviewComponet will be cleaned up by PreviewScene, 
				// but if anything is attached, it won't be cleaned up, 
				// so we'll need to clean them up manually
				CleanupComponent(SkeletalMeshComponent->GetAttachChildren()[I]);
			}
		}

		if( bRemovePreviewAttached )
		{
			check(SkeletalMeshComponent->GetAttachChildren().Num() == 0);
		}
	}
}

void FAnimationEditorPreviewScene::CleanupComponent(USceneComponent* Component)
{
	if (Component)
	{
		for (int32 I = Component->GetAttachChildren().Num() - 1; I >= 0; --I) // Iterate backwards because CleanupComponent will remove from AttachChildren
		{
			CleanupComponent(Component->GetAttachChildren()[I]);
		}

		check(Component->GetAttachChildren().Num() == 0);
		Component->DestroyComponent();
	}
}

void FAnimationEditorPreviewScene::SetPreviewAnimationAsset(UAnimationAsset* AnimAsset, bool bEnablePreview /*= true*/)
{
	if (SkeletalMeshComponent)
	{
		const USkeleton& Skeleton = GetEditableSkeleton()->GetSkeleton();

		RemoveAttachedComponent(false);

		if (AnimAsset != NULL)
		{
			// Early out if the new preview asset is the same as the current one, to avoid replaying from the beginning, etc...
			if (AnimAsset == GetPreviewAnimationAsset() && SkeletalMeshComponent->IsPreviewOn())
			{
				return;
			}

			// Treat it as invalid if it's got a bogus skeleton pointer
			if (AnimAsset->GetSkeleton() != &Skeleton)
			{
				return;
			}
		}

		CachedPreviewAsset = AnimAsset;

		SkeletalMeshComponent->EnablePreview(bEnablePreview, AnimAsset);
	}

	OnAnimChanged.Broadcast(AnimAsset);
}

UAnimationAsset* FAnimationEditorPreviewScene::GetPreviewAnimationAsset() const
{
	if (SkeletalMeshComponent)
	{
		// if same, do not overwrite. It will reset time and everything
		if (SkeletalMeshComponent->PreviewInstance != nullptr)
		{
			return SkeletalMeshComponent->PreviewInstance->GetCurrentAsset();
		}
	}

	return nullptr;
}

void FAnimationEditorPreviewScene::SetFloorLocation(const FVector& InPosition)
{
	FloorMeshComponent->SetWorldTransform(FTransform(FQuat::Identity, InPosition, FVector(3.0f, 3.0f, 1.0f)));
}

void FAnimationEditorPreviewScene::ShowReferencePose(bool bReferencePose)
{
	if(SkeletalMeshComponent)
	{
		if(bReferencePose == false)
		{
			UAnimBlueprint* AnimBP = PersonaToolkit.Pin()->GetAnimBlueprint();
			if (AnimBP)
			{
				SkeletalMeshComponent->EnablePreview(false, nullptr);
				SkeletalMeshComponent->SetAnimInstanceClass(AnimBP->GeneratedClass);
			}
			else
			{
				UObject* PreviewAsset = CachedPreviewAsset.IsValid() ? CachedPreviewAsset.Get() : (PersonaToolkit.Pin()->GetAnimationAsset());
				SkeletalMeshComponent->EnablePreview(true, Cast<UAnimationAsset>(PreviewAsset));

				if (SkeletalMeshComponent->PreviewInstance && SkeletalMeshComponent->PreviewInstance->GetCurrentAsset())
				{
					CachedPreviewAsset = SkeletalMeshComponent->PreviewInstance->GetCurrentAsset();
				}
			}
		}
		else
		{
			if (SkeletalMeshComponent->PreviewInstance && SkeletalMeshComponent->PreviewInstance->GetCurrentAsset())
			{
				CachedPreviewAsset = SkeletalMeshComponent->PreviewInstance->GetCurrentAsset();
			}
			
			SkeletalMeshComponent->EnablePreview(true, nullptr);
		}
	}
}

bool FAnimationEditorPreviewScene::IsShowReferencePoseEnabled() const
{
	return SkeletalMeshComponent->IsPreviewOn() && SkeletalMeshComponent->PreviewInstance->GetCurrentAsset() == nullptr;
}

void FAnimationEditorPreviewScene::SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode Mode, bool bShowNow)
{
	DefaultMode = Mode;

	if (bShowNow)
	{
		ShowDefaultMode();
	}
}

void FAnimationEditorPreviewScene::ShowDefaultMode()
{
	switch (DefaultMode)
	{
	case EPreviewSceneDefaultAnimationMode::ReferencePose:
		ShowReferencePose(true);
		break;
	case EPreviewSceneDefaultAnimationMode::Animation:
		{
			UObject* PreviewAsset = CachedPreviewAsset.IsValid() ? CachedPreviewAsset.Get() : (PersonaToolkit.Pin()->GetAnimationAsset());
			if (PreviewAsset)
			{
				SkeletalMeshComponent->EnablePreview(true, Cast<UAnimationAsset>(PreviewAsset));
			}
		}
		break;
	case EPreviewSceneDefaultAnimationMode::AnimationBlueprint:
		{
			SkeletalMeshComponent->EnablePreview(false, nullptr);

			UAnimBlueprint* AnimBP = PersonaToolkit.Pin()->GetAnimBlueprint();
			if (AnimBP)
			{
				SkeletalMeshComponent->SetAnimInstanceClass(AnimBP->GeneratedClass);
			}
		}
		break;
	}
}

FText FAnimationEditorPreviewScene::GetPreviewAssetTooltip(bool bEditingAnimBlueprint) const
{
	// if already looking at ref pose
	if (IsShowReferencePoseEnabled())
	{
		FText PreviewFormat(LOCTEXT("PreviewFormat", "Preview {0}"));

		if (bEditingAnimBlueprint)
		{
			UAnimBlueprint* AnimBP = PersonaToolkit.Pin()->GetAnimBlueprint();
			if (AnimBP)
			{
				return FText::Format(PreviewFormat, FText::FromString(AnimBP->GetName()));
			}
		}
		else
		{
			UObject* PreviewAsset = CachedPreviewAsset.IsValid() ? CachedPreviewAsset.Get() : (PersonaToolkit.Pin()->GetAnimationAsset());
			if (PreviewAsset)
			{
				return FText::Format(PreviewFormat, FText::FromString(PreviewAsset->GetName()));
			}
		}

		return LOCTEXT("NoPreviewAvailable", "None Available. Please select asset to preview.");
	}
	else
	{
		return FText::Format(LOCTEXT("CurrentlyPreviewingFormat", "Currently previewing {0}"), FText::FromString(SkeletalMeshComponent->GetPreviewText()));
	}
}

void FAnimationEditorPreviewScene::ClearSelectedBone()
{
	SelectedBoneIndex = INDEX_NONE;
	SkeletalMeshComponent->BonesOfInterest.Empty();

	InvalidateViews();
}

void FAnimationEditorPreviewScene::SetSelectedBone(const FName& BoneName)
{
	const int32 BoneIndex = GetEditableSkeleton()->GetSkeleton().GetReferenceSkeleton().FindBoneIndex(BoneName);
	if (BoneIndex != INDEX_NONE)
	{
		ClearSelectedBone();
		ClearSelectedSocket();
		ClearSelectedActor();

		// Add in bone of interest only if we have a preview instance set-up
		if (SkeletalMeshComponent->PreviewInstance != NULL)
		{
			// need to get mesh bone base since BonesOfInterest is saved in SkeletalMeshComponent
			// and it is used by renderer. It is not Skeleton base
			const int32 MeshBoneIndex = SkeletalMeshComponent->GetBoneIndex(BoneName);

			if (MeshBoneIndex != INDEX_NONE)
			{
				SelectedBoneIndex = MeshBoneIndex;
				SkeletalMeshComponent->BonesOfInterest.Add(SelectedBoneIndex);
			}

			InvalidateViews();
		}
	}
}

void FAnimationEditorPreviewScene::SetSelectedSocket(const FSelectedSocketInfo& SocketInfo)
{
	ClearSelectedBone();
	ClearSelectedActor();

	SelectedSocket = SocketInfo;

	InvalidateViews();
}

void FAnimationEditorPreviewScene::ClearSelectedSocket()
{
	SelectedSocket.Reset();

	InvalidateViews();
}

void FAnimationEditorPreviewScene::SetSelectedActor(AActor* InActor)
{
	ClearSelectedBone();
	ClearSelectedSocket();

	SelectedActor = InActor;

	InvalidateViews();
}

void FAnimationEditorPreviewScene::ClearSelectedActor()
{
	SelectedActor = nullptr;

	InvalidateViews();
}

void FAnimationEditorPreviewScene::DeselectAll()
{
	ClearSelectedBone();
	ClearSelectedSocket();
	ClearSelectedActor();

	InvalidateViews();
}

bool FAnimationEditorPreviewScene::IsRecordAvailable() const
{
	// make sure mesh exists
	return (SkeletalMeshComponent->SkeletalMesh != nullptr);
}

FSlateIcon FAnimationEditorPreviewScene::GetRecordStatusImage() const
{
	if (IsRecording())
	{
		return FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.StopRecordAnimation");
	}

	return FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.StartRecordAnimation");
}

FText FAnimationEditorPreviewScene::GetRecordMenuLabel() const
{
	if (IsRecording())
	{
		return LOCTEXT("Persona_StopRecordAnimationMenuLabel", "Stop Record Animation");
	}

	return LOCTEXT("Persona_StartRecordAnimationMenuLabel", "Start Record Animation");
}

FText FAnimationEditorPreviewScene::GetRecordStatusLabel() const
{
	bool bInRecording = false;
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>(TEXT("Persona"));
	PersonaModule.OnIsRecordingActive().ExecuteIfBound(SkeletalMeshComponent, bInRecording);

	if (bInRecording)
	{
		return LOCTEXT("Persona_StopRecordAnimationLabel", "Stop");
	}

	return LOCTEXT("Persona_StartRecordAnimationLabel", "Record");
}

FText FAnimationEditorPreviewScene::GetRecordStatusTooltip() const
{
	bool bInRecording = false;
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>(TEXT("Persona"));
	PersonaModule.OnIsRecordingActive().ExecuteIfBound(SkeletalMeshComponent, bInRecording);

	if (bInRecording)
	{
		return LOCTEXT("Persona_StopRecordAnimation", "Stop Record Animation");
	}

	return LOCTEXT("Persona_StartRecordAnimation", "Start Record Animation");
}

void FAnimationEditorPreviewScene::RecordAnimation()
{
	bool bInRecording = false;
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>(TEXT("Persona"));
	PersonaModule.OnIsRecordingActive().ExecuteIfBound(SkeletalMeshComponent, bInRecording);

	if (bInRecording)
	{
		PersonaModule.OnStopRecording().ExecuteIfBound(SkeletalMeshComponent);
	}
	else
	{
		PersonaModule.OnRecord().ExecuteIfBound(SkeletalMeshComponent);
	}
}

bool FAnimationEditorPreviewScene::IsRecording() const
{
	bool bInRecording = false;
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
	PersonaModule.OnIsRecordingActive().ExecuteIfBound(SkeletalMeshComponent, bInRecording);

	return bInRecording;
}

void FAnimationEditorPreviewScene::StopRecording()
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
	PersonaModule.OnStopRecording().ExecuteIfBound(SkeletalMeshComponent);
}

UAnimSequence* FAnimationEditorPreviewScene::GetCurrentRecording() const
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
	UAnimSequence* Recording = nullptr;
	PersonaModule.OnGetCurrentRecording().ExecuteIfBound(SkeletalMeshComponent, Recording);
	return Recording;
}

float FAnimationEditorPreviewScene::GetCurrentRecordingTime() const
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
	float RecordingTime = 0.0f;
	PersonaModule.OnGetCurrentRecordingTime().ExecuteIfBound(SkeletalMeshComponent, RecordingTime);
	return RecordingTime;
}

TWeakObjectPtr<AWindDirectionalSource> FAnimationEditorPreviewScene::CreateWindActor(UWorld* World)
{
	TWeakObjectPtr<AWindDirectionalSource> Wind = World->SpawnActor<AWindDirectionalSource>(PrevWindLocation, PrevWindRotation);

	check(Wind.IsValid());
	//initial wind strength value 
	Wind->GetComponent()->Strength = PrevWindStrength;
	return Wind;
}

void FAnimationEditorPreviewScene::EnableWind(bool bEnableWind)
{
	UWorld* World = GetWorld();
	check(World);

	if (bEnableWind)
	{
		if (!WindSourceActor.IsValid())
		{
			WindSourceActor = CreateWindActor(World);
		}
	}
	else if (WindSourceActor.IsValid())
	{
		PrevWindLocation = WindSourceActor->GetActorLocation();
		PrevWindRotation = WindSourceActor->GetActorRotation();
		PrevWindStrength = WindSourceActor->GetComponent()->Strength;

		World->DestroyActor(WindSourceActor.Get());
	}
}

bool FAnimationEditorPreviewScene::IsWindEnabled() const
{
	return WindSourceActor.IsValid();
}

void FAnimationEditorPreviewScene::SetWindStrength(float SliderPos)
{
	if (WindSourceActor.IsValid())
	{
		//Clamp grid size slider value between 0 - 1
		WindSourceActor->GetComponent()->Strength = FMath::Clamp<float>(SliderPos, 0.0f, 1.0f);
		//to apply this new wind strength
		WindSourceActor->UpdateComponentTransforms();
	}
}

float FAnimationEditorPreviewScene::GetWindStrength() const
{
	if (WindSourceActor.IsValid())
	{
		return WindSourceActor->GetComponent()->Strength;
	}

	return 0;
}

void FAnimationEditorPreviewScene::SetGravityScale(float InGravityScale)
{
	GravityScale = InGravityScale;
	float RealGravityScale = InGravityScale * 4.0f;

	UWorld* World = GetWorld();
	AWorldSettings* Setting = World->GetWorldSettings();
	Setting->WorldGravityZ = UPhysicsSettings::Get()->DefaultGravityZ*RealGravityScale;
	Setting->bWorldGravitySet = true;
}

float FAnimationEditorPreviewScene::GetGravityScale() const
{
	return GravityScale;
}

AActor* FAnimationEditorPreviewScene::GetSelectedActor() const
{
	return SelectedActor.Get(); 
}

FSelectedSocketInfo FAnimationEditorPreviewScene::GetSelectedSocket() const
{
	return SelectedSocket; 
}

int32 FAnimationEditorPreviewScene::GetSelectedBoneIndex() const
{ 
	return SelectedBoneIndex;
}

void FAnimationEditorPreviewScene::Tick(float InDeltaTime)
{
	IPersonaPreviewScene::Tick(InDeltaTime);

	if (LastCachedLODForPreviewComponent != SkeletalMeshComponent->PredictedLODLevel)
	{
		OnLODChanged.Broadcast();
		LastCachedLODForPreviewComponent = SkeletalMeshComponent->PredictedLODLevel;
	}
}

void FAnimationEditorPreviewScene::PostUndo(bool bSuccess)
{
	// refresh skeletal mesh
	SetPreviewMesh(GetEditableSkeleton()->GetSkeleton().GetPreviewMesh());
}

void FAnimationEditorPreviewScene::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

#undef LOCTEXT_NAMESPACE