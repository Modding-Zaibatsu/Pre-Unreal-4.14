// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorLevelUtils.cpp: Editor-specific level management routines
=============================================================================*/

#include "UnrealEd.h"
#include "EditorSupportDelegates.h"
#include "EditorLevelUtils.h"

#include "BusyCursor.h"
#include "LevelUtils.h"
#include "Layers/ILayers.h"

#include "LevelEditor.h"
#include "ScopedTransaction.h"
#include "ActorEditorUtils.h"
#include "ContentStreaming.h"
#include "PackageTools.h"

#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#include "Engine/LevelStreamingVolume.h"
#include "Engine/LevelStreaming.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Selection.h"
#include "Components/ModelComponent.h"

DEFINE_LOG_CATEGORY(LogLevelTools);

#define LOCTEXT_NAMESPACE "EditorLevelUtils"

namespace EditorLevelUtils
{
	/**
	 * Moves the specified list of actors to the specified level
	 *
	 * @param	ActorsToMove		List of actors to move
	 * @param	DestLevelStreaming	The level streaming object associated with the destination level
	 * @param	OutNumMovedActors	The number of actors that were successfully moved to the new level
	 */
	void MovesActorsToLevel( TArray< AActor* >& ActorsToMove, ULevelStreaming* DestLevelStreaming, int32& OutNumMovedActors )
	{
		// Backup the current contents of the clipboard string as we'll be using cut/paste features to move actors
		// between levels and this will trample over the clipboard data.
		FString OriginalClipboardContent;
		FPlatformMisc::ClipboardPaste(OriginalClipboardContent);

		check( DestLevelStreaming != NULL );
		ULevel* DestLevel = DestLevelStreaming->GetLoadedLevel();
		check( DestLevel != NULL );

		// Cache a the destination world
		UWorld* World = DestLevel->OwningWorld;

		// Deselect all actors
		GEditor->Exec( World, TEXT("ACTOR SELECT NONE"));

		const FString& NewLevelName = DestLevelStreaming->GetWorldAssetPackageName();
				
		for( TArray< AActor* >::TIterator CurActorIt( ActorsToMove ); CurActorIt; ++CurActorIt )
		{
			AActor* CurActor = *CurActorIt;
			check( CurActor != NULL );

			if( !FLevelUtils::IsLevelLocked( DestLevel ) && !FLevelUtils::IsLevelLocked( CurActor ) )
			{
				ULevelStreaming* ActorPrevLevel = FLevelUtils::FindStreamingLevel( CurActor->GetLevel() );
				const FString& PrevLevelName = ActorPrevLevel != NULL ? ActorPrevLevel->GetWorldAssetPackageName() : CurActor->GetLevel()->GetName();
				UE_LOG(LogLevelTools, Warning, TEXT( "AutoLevel: Moving %s from %s to %s" ), *CurActor->GetName(), *PrevLevelName, *NewLevelName );

				// Select this actor
				GEditor->SelectActor( CurActor, true, false, true );
				// Cache the world the actor is in, or if we already did ensure its the same world.
				check( World == CurActor->GetWorld() );				
			}
			else
			{
				// Either the source or destination level was locked!
				// @todo: Display warning
			}
		}
		
		OutNumMovedActors = 0;
		if( GEditor->GetSelectedActorCount() > 0 )
		{
			// @todo: Perf: Not sure if this is needed here.
			GEditor->NoteSelectionChange();

			// Move the actors!
			GEditor->MoveSelectedActorsToLevel( DestLevel );

			// The moved (pasted) actors will now be selected
			OutNumMovedActors += GEditor->GetSelectedActorCount();
		}
		
		// Restore the original clipboard contents
		FPlatformMisc::ClipboardCopy( *OriginalClipboardContent );
	}

	static TArray<FString> GStreamingMethodStrings;
	static TArray<UClass*> GStreamingMethodClassList;

	/**
	 * Initializes the list of possible level streaming methods. 
	 * Does nothing if the lists are already initialized.
	 */
	void InitializeStreamingMethods()
	{
		check( GStreamingMethodStrings.Num() == GStreamingMethodClassList.Num() );
		if ( GStreamingMethodClassList.Num() == 0 )
		{
			// Assemble a list of possible level streaming methods.
			for ( TObjectIterator<UClass> It ; It ; ++It )
			{
				if ( It->IsChildOf( ULevelStreaming::StaticClass() ) &&
					(It->HasAnyClassFlags(CLASS_EditInlineNew)) &&
					!(It->HasAnyClassFlags(CLASS_Hidden | CLASS_Abstract | CLASS_Deprecated | CLASS_Transient)))
				{
					const FString ClassName( It->GetName() );
					// Strip the leading "LevelStreaming" text from the class name.
					// @todo DB: This assumes the names of all ULevelStreaming-derived types begin with the string "LevelStreaming".
					GStreamingMethodStrings.Add( ClassName.Mid( 14 ) );
					GStreamingMethodClassList.Add( *It );
				}
			}
		}
	}

	ULevel* AddLevelsToWorld(UWorld* InWorld, const TArray<FString>& LevelPackageNames, UClass* LevelStreamingClass)
	{
		if ( !ensure(InWorld) )
		{
			return nullptr;
		}

		FScopedSlowTask SlowTask(LevelPackageNames.Num(), LOCTEXT("AddLevelsToWorldTask", "Adding Levels to World"));
		SlowTask.MakeDialog();

		TArray<FString> PackageNames = LevelPackageNames;

		// Sort the level packages alphabetically by name.
		PackageNames.Sort();

		// Fire ULevel::LevelDirtiedEvent when falling out of scope.
		FScopedLevelDirtied LevelDirtyCallback;

		// Try to add the levels that were specified in the dialog.
		ULevel* NewLevel = nullptr;
		for (const auto& PackageName : PackageNames)
		{
			SlowTask.EnterProgressFrame();

			NewLevel = AddLevelToWorld(InWorld, *PackageName, LevelStreamingClass);
			if (NewLevel)
			{
				LevelDirtyCallback.Request();
			}

		} // for each file

		// Set the last loaded level to be the current level
		if (NewLevel)
		{
			InWorld->SetCurrentLevel(NewLevel);
		}

		// For safety
		if (GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Landscape))
		{
			GLevelEditorModeTools().ActivateDefaultMode();
		}

		// refresh editor windows
		FEditorDelegates::RefreshAllBrowsers.Broadcast();

		// Update volume actor visibility for each viewport since we loaded a level which could potentially contain volumes
		GUnrealEd->UpdateVolumeActorVisibility(NULL);

		return NewLevel;
	}

	ULevel* AddLevelToWorld( UWorld* InWorld, const TCHAR* LevelPackageName, UClass* LevelStreamingClass)
	{
		ULevel* NewLevel = NULL;
		bool bIsPersistentLevel = (InWorld->PersistentLevel->GetOutermost()->GetName() == FString(LevelPackageName));

		if ( bIsPersistentLevel || FLevelUtils::FindStreamingLevel( InWorld, LevelPackageName ) )
		{
			// Do nothing if the level already exists in the world.
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "LevelAlreadyExistsInWorld", "A level with that name already exists in the world.") );
		}
		else
		{
			// If the selected class is still NULL, abort the operation.
			if ( LevelStreamingClass == NULL )
			{
				return NULL;
			}	

			const FScopedBusyCursor BusyCursor;

			ULevelStreaming* StreamingLevel = NewObject<ULevelStreaming>(InWorld, LevelStreamingClass, NAME_None, RF_NoFlags, NULL);

			// Associate a package name.
			StreamingLevel->SetWorldAssetByPackageName(LevelPackageName);

			// Seed the level's draw color.
			StreamingLevel->LevelColor = FLinearColor::MakeRandomColor();

			// Add the new level to world.
			InWorld->StreamingLevels.Add( StreamingLevel );

			// Refresh just the newly created level.
			TArray<ULevelStreaming*> LevelsForRefresh;
			LevelsForRefresh.Add( StreamingLevel );
			InWorld->RefreshStreamingLevels( LevelsForRefresh );
			InWorld->MarkPackageDirty();

			NewLevel = StreamingLevel->GetLoadedLevel();
			if ( NewLevel != nullptr)
			{
				EditorLevelUtils::SetLevelVisibility( NewLevel, true, true );

				// Levels migrated from other projects may fail to load their world settings
				// If so we create a new AWorldSettings actor here.
				if ( NewLevel->GetWorldSettings(false) == nullptr )
				{
					UWorld* SubLevelWorld = CastChecked<UWorld>(NewLevel->GetOuter());

					FActorSpawnParameters SpawnInfo;
					SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnInfo.Name = GEngine->WorldSettingsClass->GetFName();
					AWorldSettings* NewWorldSettings = SubLevelWorld->SpawnActor<AWorldSettings>( GEngine->WorldSettingsClass, SpawnInfo );

					NewLevel->SetWorldSettings(NewWorldSettings);
				}
			}
		}

		if(NewLevel) // if the level was successfully added
		{
			FEditorDelegates::OnAddLevelToWorld.Broadcast(NewLevel);
		}

		return NewLevel;
	}

	ULevelStreaming* SetStreamingClassForLevel(ULevelStreaming* InLevel, UClass* LevelStreamingClass)
	{
		check(InLevel);

		const FScopedBusyCursor BusyCursor;

		// Cache off the package name, as it will be lost when unloading the level
		const FName CachedPackageName = InLevel->GetWorldAssetPackageFName();

		// First hide and remove the level if it exists
		ULevel* Level = InLevel->GetLoadedLevel();
		check(Level);
		SetLevelVisibility( Level, false, false );
		check(Level->OwningWorld);
		UWorld* World = Level->OwningWorld;
		
		World->StreamingLevels.Remove(InLevel);
		
		// re-add the level with the desired streaming class
		AddLevelToWorld(World, *(CachedPackageName.ToString()), LevelStreamingClass);

		// Transfer level streaming settings
		ULevelStreaming* NewStreamingLevel = FLevelUtils::FindStreamingLevel( Level );
		if ( NewStreamingLevel )
		{
			NewStreamingLevel->LevelTransform = InLevel->LevelTransform;
			NewStreamingLevel->EditorStreamingVolumes = InLevel->EditorStreamingVolumes;
			NewStreamingLevel->MinTimeBetweenVolumeUnloadRequests = InLevel->MinTimeBetweenVolumeUnloadRequests;
			NewStreamingLevel->LevelColor = InLevel->LevelColor;
			NewStreamingLevel->Keywords = InLevel->Keywords;
		}

		return NewStreamingLevel;
	}

	void MakeLevelCurrent(ULevel* InLevel)
	{
		// Locked levels can't be made current.
		if ( !FLevelUtils::IsLevelLocked( InLevel ) )
		{ 
			// Make current broadcast if it changed
			if( InLevel->OwningWorld->SetCurrentLevel( InLevel ) )
			{
				FEditorDelegates::NewCurrentLevel.Broadcast();
			}
			
			// Deselect all selected builder brushes.
			bool bDeselectedSomething = false;
			for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );
				ABrush* Brush = Cast< ABrush >( Actor );
				if ( Brush && FActorEditorUtils::IsABuilderBrush(Actor) )
				{
					GEditor->SelectActor( Actor, /*bInSelected=*/ false, /*bNotify=*/ false );
					bDeselectedSomething = true;
				}
			}

			// Send a selection change callback if necessary.
			if ( bDeselectedSomething )
			{
				GEditor->NoteSelectionChange();
			}

			// Force the current level to be visible.
			SetLevelVisibility( InLevel, true, false );
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_OperationDisallowedOnLockedLevelMakeLevelCurrent", "MakeLevelCurrent: The requested operation could not be completed because the level is locked.") );
		}
	}
	
	/**
	 * Removes a LevelStreaming from the world. Returns true if the LevelStreaming was removed successfully.
	 *
	 * @param	InLevelStreaming	The LevelStreaming to remove.
	 * @return						true if the LevelStreaming was removed successfully, false otherwise.
	 */
	bool PrivateRemoveInvalidLevelFromWorld(ULevelStreaming* InLevelStreaming)
	{
		bool bRemovedLevelStreaming = false;
		if (InLevelStreaming != NULL)
		{
			check(InLevelStreaming->GetLoadedLevel() == NULL); // This method is designed to be used to remove left over references to null levels 

			InLevelStreaming->Modify();

			// Disassociate the level from the volume.
			for ( auto VolIter = InLevelStreaming->EditorStreamingVolumes.CreateIterator(); VolIter; VolIter++ )
			{
				ALevelStreamingVolume* LevelStreamingVolume = *VolIter;
				if ( LevelStreamingVolume )
				{
					LevelStreamingVolume->Modify();
					LevelStreamingVolume->StreamingLevelNames.Remove( InLevelStreaming->GetWorldAssetPackageFName() );
				}
			}

			// Disassociate the volumes from the level.
			InLevelStreaming->EditorStreamingVolumes.Empty();

			UWorld* OwningWorld = Cast<UWorld>(InLevelStreaming->GetOuter());
			if (OwningWorld != NULL)
			{
				OwningWorld->StreamingLevels.Remove(InLevelStreaming);
				OwningWorld->RefreshStreamingLevels();
				bRemovedLevelStreaming = true;
			}
		}
		return bRemovedLevelStreaming;
	}

	bool RemoveInvalidLevelFromWorld(ULevelStreaming* InLevelStreaming)
	{
		bool bRemoveSuccessful = PrivateRemoveInvalidLevelFromWorld(InLevelStreaming);
		if (bRemoveSuccessful)
		{
			// Redraw the main editor viewports.
			FEditorSupportDelegates::RedrawAllViewports.Broadcast();

			// refresh editor windows
			FEditorDelegates::RefreshAllBrowsers.Broadcast();

			// Update selection for any selected actors that were in the level and are no longer valid
			GEditor->NoteSelectionChange();

			// Collect garbage to clear out the destroyed level
			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
		}
		return bRemoveSuccessful;
	}
	
	bool RemoveLevelFromWorld(ULevel* InLevel)
	{
		if (GEditor->Layers.IsValid())
		{
			GEditor->Layers->RemoveLevelLayerInformation( InLevel );
		}
		
		GEditor->CloseEditedWorldAssets(CastChecked<UWorld>(InLevel->GetOuter()));

		UWorld* OwningWorld = InLevel->OwningWorld;
		const FName LevelPackageName		= InLevel->GetOutermost()->GetFName();
		const bool bRemovingCurrentLevel	= InLevel->IsCurrentLevel();
		const bool bRemoveSuccessful		= PrivateRemoveLevelFromWorld( InLevel );
		if ( bRemoveSuccessful )
		{
			if (bRemovingCurrentLevel)
			{
				MakeLevelCurrent(OwningWorld->PersistentLevel);
			}


			EditorDestroyLevel(InLevel);

			// Redraw the main editor viewports.
			FEditorSupportDelegates::RedrawAllViewports.Broadcast();

			// refresh editor windows
			FEditorDelegates::RefreshAllBrowsers.Broadcast();
			
			// Reset transaction buffer and run GC to clear out the destroyed level
			GEditor->Cleanse(true, false, LOCTEXT("RemoveLevelTransReset", "Removing Levels from World"));

			// Ensure that world was removed
			UPackage* LevelPackage = FindObjectFast<UPackage>(NULL, LevelPackageName);
			if (LevelPackage != nullptr)
			{
				UWorld* TheWorld = UWorld::FindWorldInPackage(LevelPackage->GetOutermost());								
				if (TheWorld != nullptr)
				{	
					StaticExec(NULL, *FString::Printf(TEXT("OBJ REFS CLASS=%s NAME=%s shortest"),*TheWorld->GetClass()->GetName(), *TheWorld->GetPathName()));
					TMap<UObject*,UProperty*>	Route		= FArchiveTraceRoute::FindShortestRootPath( TheWorld, true, GARBAGE_COLLECTION_KEEPFLAGS );
					FString						ErrorString	= FArchiveTraceRoute::PrintRootPath( Route, TheWorld );
					UE_LOG(LogStreaming, Fatal, TEXT("%s didn't get garbage collected!") LINE_TERMINATOR TEXT("%s"), *TheWorld->GetFullName(), *ErrorString );
				}
			}
		}
		return bRemoveSuccessful;
	}


	/**
	* Removes a level from the world.  Returns true if the level was removed successfully.
	*
	* @param	InLevel		The level to remove from the world.
	* @return				true if the level was removed successfully, false otherwise.
	*/
	bool PrivateRemoveLevelFromWorld(ULevel* InLevel)
	{
		if ( !InLevel || InLevel->IsPersistentLevel() )
		{
			return false;
		}

		if ( FLevelUtils::IsLevelLocked(InLevel) )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_OperationDisallowedOnLockedLevelRemoveLevelFromWorld", "RemoveLevelFromWorld: The requested operation could not be completed because the level is locked.") );
			return false;
		}

		int32 StreamingLevelIndex = INDEX_NONE;

		for( int32 LevelIndex = 0 ; LevelIndex < InLevel->OwningWorld->StreamingLevels.Num() ; ++LevelIndex )
		{
			ULevelStreaming* StreamingLevel = InLevel->OwningWorld->StreamingLevels[ LevelIndex ];
			if( StreamingLevel && StreamingLevel->GetLoadedLevel() == InLevel )
			{
				StreamingLevelIndex = LevelIndex;
				break;
			}
		}

		if (StreamingLevelIndex != INDEX_NONE)
		{
			InLevel->OwningWorld->StreamingLevels[StreamingLevelIndex]->MarkPendingKill();
			InLevel->OwningWorld->StreamingLevels.RemoveAt( StreamingLevelIndex );
			InLevel->OwningWorld->RefreshStreamingLevels();
		}
		else if (InLevel->bIsVisible)
		{
			InLevel->OwningWorld->RemoveFromWorld(InLevel);
			check(InLevel->bIsVisible == false);
		}

		InLevel->ReleaseRenderingResources();

		IStreamingManager::Get().RemoveLevel( InLevel );
		UWorld* World = InLevel->OwningWorld;
		World->RemoveLevel(InLevel);
		InLevel->ClearLevelComponents();

		// remove all group actors from the world in the level we are removing
		// otherwise, this will cause group actors to not be garbage collected
		for (int32 GroupIndex = World->ActiveGroupActors.Num()-1; GroupIndex >= 0; --GroupIndex)
		{
			AGroupActor* GroupActor = Cast<AGroupActor>(World->ActiveGroupActors[GroupIndex]);
			if (GroupActor && GroupActor->IsInLevel(InLevel))
			{
				World->ActiveGroupActors.RemoveAt(GroupIndex);
			}
		}
		
		// Mark all model components as pending kill so GC deletes references to them.
		for (UModelComponent* ModelComponent : InLevel->ModelComponents)
		{
			if (ModelComponent != nullptr)
			{
				ModelComponent->MarkPendingKill();
			}
		}

		// Mark all actors and their components as pending kill so GC will delete references to them.
		for (AActor* Actor : InLevel->Actors)
		{
			if (Actor != nullptr)
			{
				Actor->MarkComponentsAsPendingKill();
				Actor->MarkPendingKill();
			}
		}

		World->MarkPackageDirty();
		World->BroadcastLevelsChanged();

		return true;
	}

	bool EditorDestroyLevel(ULevel* InLevel)
	{
		UWorld* World = InLevel->OwningWorld;

		InLevel->GetOuter()->MarkPendingKill();
		InLevel->MarkPendingKill();
		InLevel->GetOuter()->ClearFlags(RF_Public | RF_Standalone);

		UPackage* Package = Cast<UPackage>(InLevel->GetOutermost());
		// We want to unconditionally destroy the level, so clear the dirty flag here so it can be unloaded successfully
		Package->SetDirtyFlag(false);

		TArray<UPackage*> Packages;
		Packages.Add(Package);
		if (!PackageTools::UnloadPackages(Packages))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Package"), FText::FromString(Package->GetName()));
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("UnloadPackagesFail", "Unable to unload package '{Package}'."), Args));
			return false;
		}

		return true;
	}

	ULevel* CreateNewLevel( UWorld* InWorld, bool bMoveSelectedActorsIntoNewLevel, UClass* LevelStreamingClass, const FString& DefaultFilename )
	{
		// Editor modes cannot be active when any level saving occurs.
		GLevelEditorModeTools().DeactivateAllModes();

		UWorld* NewWorld = nullptr;
		if (UEditorEngine::IsUsingWorldAssets())
		{
			// Create a new world
			UWorldFactory* Factory = NewObject<UWorldFactory>();
			Factory->WorldType = EWorldType::Inactive;
			UPackage* Pkg = CreatePackage(NULL, NULL);
			FName WorldName(TEXT("Untitled"));
			EObjectFlags Flags = RF_Public | RF_Standalone;
			NewWorld = CastChecked<UWorld>(Factory->FactoryCreateNew(UWorld::StaticClass(), Pkg, WorldName, Flags, NULL, GWarn));
			if ( NewWorld )
			{
				FAssetRegistryModule::AssetCreated(NewWorld);
			}
		}
		else
		{
			// Create a new world - so we can 'borrow' its level
			NewWorld = UWorld::CreateWorld(EWorldType::None, false );
			check(NewWorld);
		}

		// Save the new world to disk.
		const bool bNewWorldSaved = FEditorFileUtils::SaveLevel( NewWorld->PersistentLevel, DefaultFilename );
		FString NewPackageName;
		if ( bNewWorldSaved )
		{
			NewPackageName = NewWorld->GetOutermost()->GetName();
		}

		if (!UEditorEngine::IsUsingWorldAssets())
		{
			// Destroy the new world we created and collect the garbage
			NewWorld->DestroyWorld( false );
			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
		}

		// If the new world was saved successfully, import it as a streaming level.
		ULevel* NewLevel = NULL;
		if ( bNewWorldSaved )
		{
			NewLevel = AddLevelToWorld( InWorld, *NewPackageName, LevelStreamingClass );

			// If we are moving the selected actors to the new level move them now
			if ( bMoveSelectedActorsIntoNewLevel )
			{
				GEditor->MoveSelectedActorsToLevel( NewLevel );
			}
			// Finally make the new level the current one
			InWorld->SetCurrentLevel( NewLevel );
		}

		// Broadcast the levels have changed (new style)
		InWorld->BroadcastLevelsChanged();
		return NewLevel;
	}
	
	void DeselectAllSurfacesInLevel(ULevel* InLevel)
	{
		if(InLevel)
		{
			UModel* Model = InLevel->Model;
			for( int32 SurfaceIndex = 0 ; SurfaceIndex < Model->Surfs.Num() ; ++SurfaceIndex )
			{
				FBspSurf& Surf = Model->Surfs[SurfaceIndex];
				if( Surf.PolyFlags & PF_Selected )
				{
					Model->ModifySurf( SurfaceIndex, false );
					Surf.PolyFlags &= ~PF_Selected;
				}
			}
		}
	}

	void SetLevelVisibility(ULevel* Level, bool bShouldBeVisible, bool bForceLayersVisible)
	{	
		// Nothing to do
		if ( Level == NULL )
		{
			return;
		}


		// Handle the case of the p-level
		// The p-level can't be unloaded, so its actors/BSP should just be temporarily hidden/unhidden
		// Also, intentionally do not force layers visible for the p-level
		if ( Level->IsPersistentLevel() )
		{
			//create a transaction so we can undo the visibilty toggle
			const FScopedTransaction Transaction( LOCTEXT( "ToggleLevelVisibility", "Toggle Level Visibility" ) );		
			if ( Level->bIsVisible != bShouldBeVisible )
			{
				Level->Modify();
			}
			// Set the visibility of each actor in the p-level
			for ( TArray<AActor*>::TIterator PLevelActorIter( Level->Actors ); PLevelActorIter; ++PLevelActorIter )
			{
				AActor* CurActor = *PLevelActorIter;
				if ( CurActor && !FActorEditorUtils::IsABuilderBrush(CurActor) && CurActor->bHiddenEdLevel == bShouldBeVisible )
				{
					CurActor->Modify();
					CurActor->bHiddenEdLevel = !bShouldBeVisible;
					CurActor->RegisterAllComponents();
					CurActor->MarkComponentsRenderStateDirty();
				}
			}

			// Set the visibility of each BSP surface in the p-level
			UModel* CurLevelModel = Level->Model;
			if ( CurLevelModel )
			{
				CurLevelModel->Modify();
				for ( TArray<FBspSurf>::TIterator SurfaceIterator( CurLevelModel->Surfs ); SurfaceIterator; ++SurfaceIterator )
				{
					FBspSurf& CurSurf = *SurfaceIterator;
					CurSurf.bHiddenEdLevel = !bShouldBeVisible;
				}
			}

			// Add/remove model components from the scene
			for(int32 ComponentIndex = 0;ComponentIndex < Level->ModelComponents.Num();ComponentIndex++)
			{
				UModelComponent* CurLevelModelCmp = Level->ModelComponents[ComponentIndex];
				if(CurLevelModelCmp)
				{
					if (bShouldBeVisible)
					{
						CurLevelModelCmp->RegisterComponentWithWorld(Level->OwningWorld);
					}
					else if (!bShouldBeVisible && CurLevelModelCmp->IsRegistered())
					{
						CurLevelModelCmp->UnregisterComponent();
					}
				}
			}

			FEditorSupportDelegates::RedrawAllViewports.Broadcast();
		}
		else
		{
			ULevelStreaming* StreamingLevel = NULL;
			if (Level->OwningWorld == NULL || Level->OwningWorld->PersistentLevel != Level )
			{
				StreamingLevel = FLevelUtils::FindStreamingLevel( Level );
			}

			// Create a transaction so we can undo the visibility toggle
			const FScopedTransaction Transaction( LOCTEXT( "ToggleLevelVisibility", "Toggle Level Visibility" ) );		

			// Handle the case of a streaming level
			if ( StreamingLevel )
			{
				// We need to set the RF_Transactional to make a streaming level serialize itself. so store the original ones, set the flag, and put the original flags back when done
				EObjectFlags cachedFlags = StreamingLevel->GetFlags();
				StreamingLevel->SetFlags( RF_Transactional );
				StreamingLevel->Modify();			
				StreamingLevel->SetFlags( cachedFlags );

				// Set the visibility state for this streaming level.  
				StreamingLevel->bShouldBeVisibleInEditor = bShouldBeVisible;
			}

			if (!bShouldBeVisible && GEditor->Layers.IsValid())
			{
				GEditor->Layers->RemoveLevelLayerInformation( Level );
			}

			// UpdateLevelStreaming sets Level->bIsVisible directly, so we need to make sure it gets saved to the transaction buffer.
			if ( Level->bIsVisible != bShouldBeVisible )
			{
				Level->Modify();
			}

			if ( StreamingLevel )
			{
				Level->OwningWorld->FlushLevelStreaming();

				// In the Editor we expect this operation will complete in a single call
				check(Level->bIsVisible == bShouldBeVisible);
			}
			else if (Level->OwningWorld != NULL)
			{
				// In case we level has no associated StreamingLevel, remove or add to world directly
				if (bShouldBeVisible)
				{
					if (!Level->bIsVisible)
					{
						Level->OwningWorld->AddToWorld(Level);
					}
				}
				else
				{
					Level->OwningWorld->RemoveFromWorld(Level);
				}
				
				// In the Editor we expect this operation will complete in a single call
				check(Level->bIsVisible == bShouldBeVisible);
			}

			if (bShouldBeVisible && GEditor->Layers.IsValid())
			{
				GEditor->Layers->AddLevelLayerInformation( Level );
			}

			// Force the level's layers to be visible, if desired
			FEditorSupportDelegates::RedrawAllViewports.Broadcast();

			// Iterate over the level's actors, making a list of their layers and unhiding the layers.
			TArray<AActor*>& Actors = Level->Actors;
			for ( int32 ActorIndex = 0 ; ActorIndex < Actors.Num() ; ++ActorIndex )
			{
				AActor* Actor = Actors[ ActorIndex ];
				if ( Actor )
				{
					bool bModified = false;
					if (bShouldBeVisible && bForceLayersVisible && 
						GEditor->Layers.IsValid() && 
						GEditor->Layers->IsActorValidForLayer( Actor ))
					{
						// Make the actor layer visible, if it's not already.
						if ( Actor->bHiddenEdLayer )
						{
							bModified = Actor->Modify();
							Actor->bHiddenEdLayer = false;
						}

						const bool bIsVisible = true;
						GEditor->Layers->SetLayersVisibility( Actor->Layers, bIsVisible );
					}

					// Set the visibility of each actor in the streaming level
					if ( !FActorEditorUtils::IsABuilderBrush(Actor) && Actor->bHiddenEdLevel == bShouldBeVisible )
					{
						if ( !bModified )
						{
							bModified = Actor->Modify();
						}
						Actor->bHiddenEdLevel = !bShouldBeVisible;

						if (bShouldBeVisible)
						{
							Actor->ReregisterAllComponents();
						}
						else
						{
							Actor->UnregisterAllComponents();
						}
					}
				}
			}
		}

		FEditorDelegates::RefreshLayerBrowser.Broadcast();

		// Notify the Scene Outliner, as new Actors may be present in the world.
		GEngine->BroadcastLevelActorListChanged();

		// If the level is being hidden, deselect actors and surfaces that belong to this level.
		if ( !bShouldBeVisible )
		{
			USelection* SelectedActors = GEditor->GetSelectedActors();
			SelectedActors->Modify();
			TArray<AActor*>& Actors = Level->Actors;
			for ( int32 ActorIndex = 0 ; ActorIndex < Actors.Num() ; ++ActorIndex )
			{
				AActor* Actor = Actors[ ActorIndex ];
				if ( Actor )
				{
					SelectedActors->Deselect( Actor );
				}
			}

			DeselectAllSurfacesInLevel(Level);

			// Tell the editor selection status was changed.
			GEditor->NoteSelectionChange();
		}

		Level->bIsVisible = bShouldBeVisible;
	}

	/**
	* Assembles the set of all referenced worlds.
	*
	* @param	OutWorlds			[out] The set of referenced worlds.
	* @param	bIncludeGWorld		If true, include GWorld in the output list.
	* @param	bOnlyEditorVisible	If true, only sub-levels that should be visible in-editor are included
	*/
	void GetWorlds(UWorld* InWorld, TArray<UWorld*>& OutWorlds, bool bIncludeInWorld, bool bOnlyEditorVisible)
	{
		OutWorlds.Empty();

		if (!InWorld)
		{
			return;
		}

		if ( bIncludeInWorld )
		{
			OutWorlds.AddUnique( InWorld );
		}

		// Iterate over the world's level array to find referenced levels ("worlds"). We don't 
		for ( int32 LevelIndex = 0 ; LevelIndex < InWorld->StreamingLevels.Num() ; ++LevelIndex )
		{
			ULevelStreaming* StreamingLevel = InWorld->StreamingLevels[LevelIndex];
			if ( StreamingLevel )
			{
				// If we asked for only sub-levels that are editor-visible, then limit our results appropriately
				bool bShouldAlwaysBeLoaded = false; // Cast< ULevelStreamingAlwaysLoaded >( StreamingLevel ) != NULL;
				if( !bOnlyEditorVisible || bShouldAlwaysBeLoaded || StreamingLevel->bShouldBeVisibleInEditor )
				{
					const ULevel* Level = StreamingLevel->GetLoadedLevel();
					
					// This should always be the case for valid level names as the Editor preloads all packages.
					if ( Level != NULL )
					{
						// Newer levels have their packages' world as the outer.
						UWorld* World = Cast<UWorld>( Level->GetOuter() );
						if ( World != NULL )
						{
							OutWorlds.AddUnique( World );
						}
					}
				}
			}
		}
		
		// Levels can be loaded directly without StreamingLevel facilities
		for ( int32 LevelIndex = 0 ; LevelIndex < InWorld->GetLevels().Num() ; ++LevelIndex )
		{
			ULevel* Level = InWorld->GetLevel(LevelIndex);
			if ( Level )
			{
				// Newer levels have their packages' world as the outer.
				UWorld* World = Cast<UWorld>( Level->GetOuter() );
				if ( World != NULL )
				{
					OutWorlds.AddUnique( World );
				}
			}
		}
	}

}

#undef LOCTEXT_NAMESPACE
