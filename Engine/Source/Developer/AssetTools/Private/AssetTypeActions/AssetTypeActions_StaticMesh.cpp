// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"

#include "Editor/StaticMeshEditor/Public/StaticMeshEditorModule.h"

#include "Editor/DestructibleMeshEditor/Public/DestructibleMeshEditorModule.h"
#include "Editor/DestructibleMeshEditor/Public/IDestructibleMeshEditor.h"

#include "ApexDestructibleAssetImport.h"
#include "Engine/DestructibleMesh.h"

#include "FbxMeshUtils.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

static TAutoConsoleVariable<int32> CVarEnableSaveGeneratedLODsInPackage(
	TEXT("r.StaticMesh.EnableSaveGeneratedLODsInPackage"),
	0,
	TEXT("Enables saving generated LODs in the Package.\n") \
	TEXT("0 - Do not save (and hide this menu option) [default].\n") \
	TEXT("1 - Enable this option and save the LODs in the Package.\n"),
	ECVF_Default);

void FAssetTypeActions_StaticMesh::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Meshes = GetTypedWeakObjectPtrs<UStaticMesh>(InObjects);

	if (CVarEnableSaveGeneratedLODsInPackage.GetValueOnGameThread() != 0)
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("AssetTypeActions_StaticMesh", "ObjectContext_SaveGeneratedLODsInPackage", "Save Generated LODs"),
			NSLOCTEXT("AssetTypeActions_StaticMesh", "ObjectContext_SaveGeneratedLODsInPackageTooltip", "Run the mesh reduce and save the generated LODs as part of the package."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::ExecuteSaveGeneratedLODsInPackage, Meshes),
				FCanExecuteAction()
				)
			);
	}

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_StaticMesh", "ObjectContext_CreateDestructibleMesh", "Create Destructible Mesh"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "ObjectContext_CreateDestructibleMeshTooltip", "Creates a DestructibleMesh from the StaticMesh and opens it in the DestructibleMesh editor."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.DestructibleComponent"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_StaticMesh::ExecuteCreateDestructibleMesh, Meshes ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddSubMenu(
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_LODMenu", "Level Of Detail"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_LODTooltip", "LOD Options and Tools"),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_StaticMesh::GetLODMenu, Meshes),
		false,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions")
		);
}

void FAssetTypeActions_StaticMesh::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Mesh = Cast<UStaticMesh>(*ObjIt);
		if (Mesh != NULL)
		{
			IStaticMeshEditorModule* StaticMeshEditorModule = &FModuleManager::LoadModuleChecked<IStaticMeshEditorModule>( "StaticMeshEditor" );
			StaticMeshEditorModule->CreateStaticMeshEditor(Mode, EditWithinLevelEditor, Mesh);
		}
	}
}

UThumbnailInfo* FAssetTypeActions_StaticMesh::GetThumbnailInfo(UObject* Asset) const
{
	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(Asset);
	UThumbnailInfo* ThumbnailInfo = StaticMesh->ThumbnailInfo;
	if ( ThumbnailInfo == NULL )
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(StaticMesh);
		StaticMesh->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_StaticMesh::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto StaticMesh = CastChecked<UStaticMesh>(Asset);
		StaticMesh->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}

void FAssetTypeActions_StaticMesh::GetImportLODMenu(class FMenuBuilder& MenuBuilder,TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	check(Objects.Num() > 0);
	auto First = Objects[0];
	UStaticMesh* StaticMesh = First.Get();

	for(int32 LOD = 1;LOD<=First->GetNumLODs();++LOD)
	{
		FText LODText = FText::AsNumber( LOD );
		FText Description = FText::Format( NSLOCTEXT("AssetTypeActions_StaticMesh", "Reimport LOD (number)", "Reimport LOD {0}"), LODText );
		FText ToolTip = NSLOCTEXT("AssetTypeActions_StaticMesh", "ReimportTip", "Reimport over existing LOD");
		if(LOD == First->GetNumLODs())
		{
			Description = FText::Format( NSLOCTEXT("AssetTypeActions_StaticMesh", "LOD (number)", "LOD {0}"), LODText );
			ToolTip = NSLOCTEXT("AssetTypeActions_StaticMesh", "NewImportTip", "Import new LOD");
		}

		MenuBuilder.AddMenuEntry( Description, ToolTip, FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic( &FbxMeshUtils::ImportMeshLODDialog, Cast<UObject>(StaticMesh), LOD) )) ;
	}
}

void FAssetTypeActions_StaticMesh::GetLODMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<UStaticMesh>> Meshes)
{

	MenuBuilder.AddSubMenu(
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_ImportLOD", "Import LOD"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_ImportLODtooltip", "Imports meshes into the LODs"),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_StaticMesh::GetImportLODMenu, Meshes)
		);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_CopyLOD", "Copy LOD"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_CopyLODTooltip", "Copies the LOD settings from the selected mesh."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::ExecuteCopyLODSettings, Meshes),
		FCanExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::CanCopyLODSettings, Meshes)
		)
		);

	FText PasteLabel = FText(LOCTEXT("StaticMesh_PasteLOD", "Paste LOD"));
	if (LODCopyMesh.IsValid())
	{
		PasteLabel = FText::Format(LOCTEXT("StaticMesh_PasteLODWithName", "Paste LOD from {0}"), FText::FromString(LODCopyMesh->GetName()));
	}
	
	MenuBuilder.AddMenuEntry(
		PasteLabel,
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_PasteLODToltip", "Pastes LOD settings to the selected mesh(es)."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::ExecutePasteLODSettings, Meshes),
		FCanExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::CanPasteLODSettings, Meshes)
		)
		);


}

void FAssetTypeActions_StaticMesh::ExecuteCopyLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	LODCopyMesh = Objects[0];
}


bool FAssetTypeActions_StaticMesh::CanCopyLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects) const
{
	return Objects.Num() == 1;
}

void FAssetTypeActions_StaticMesh::ExecutePasteLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	if (!LODCopyMesh.IsValid())
	{
		return;
	}

	// Retrieve LOD settings from source mesh
	struct FLODSettings
	{
		FMeshReductionSettings		ReductionSettings;
		float						ScreenSize;
	};

	TArray<FLODSettings> LODSettings;
	LODSettings.AddZeroed(LODCopyMesh->SourceModels.Num());
	for (int32 i = 0; i < LODCopyMesh->SourceModels.Num(); i++)
	{
		LODSettings[i].ReductionSettings = LODCopyMesh->SourceModels[i].ReductionSettings;
		LODSettings[i].ScreenSize = LODCopyMesh->SourceModels[i].ScreenSize;
	}

	const bool bAutoComputeLODScreenSize = LODCopyMesh->bAutoComputeLODScreenSize;
	const float StreamingDistanceMultiplier = LODCopyMesh->StreamingDistanceMultiplier;

	// Copy LOD settings over to selected objects in content browser (meshes)
	for (TWeakObjectPtr<UStaticMesh> MeshPtr : Objects)
	{
		if (MeshPtr.IsValid())
		{
			UStaticMesh* Mesh = MeshPtr.Get();

			const int32 LODCount = LODSettings.Num();
			if (Mesh->SourceModels.Num() > LODCount)
			{
				const int32 NumToRemove = Mesh->SourceModels.Num() - LODCount;
				Mesh->SourceModels.RemoveAt(LODCount, NumToRemove);
			}

			while (Mesh->SourceModels.Num() < LODCount)
			{
				new(Mesh->SourceModels) FStaticMeshSourceModel();
			}

			for (int32 i = 0; i < LODCount; i++)
			{
				Mesh->SourceModels[i].ReductionSettings = LODSettings[i].ReductionSettings;
				Mesh->SourceModels[i].ScreenSize = LODSettings[i].ScreenSize;
			}

			for (int32 i = LODCount; i < LODSettings.Num(); ++i)
			{
				FStaticMeshSourceModel& SrcModel = Mesh->SourceModels[i];

				SrcModel.ReductionSettings = LODSettings[i].ReductionSettings;
				SrcModel.ScreenSize = LODSettings[i].ScreenSize;
			}

			Mesh->bAutoComputeLODScreenSize = bAutoComputeLODScreenSize;
			Mesh->StreamingDistanceMultiplier = StreamingDistanceMultiplier;

			Mesh->PostEditChange();
			Mesh->MarkPackageDirty();
		}
	}

}

bool FAssetTypeActions_StaticMesh::CanPasteLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects) const
{
	return LODCopyMesh.IsValid();
}

void FAssetTypeActions_StaticMesh::ExecuteCreateDestructibleMesh(TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	TArray< UObject* > Assets;
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			FText ErrorMsg;
			FDestructibleMeshEditorModule& DestructibleMeshEditorModule = FModuleManager::LoadModuleChecked<FDestructibleMeshEditorModule>( "DestructibleMeshEditor" );
			UDestructibleMesh* DestructibleMesh = DestructibleMeshEditorModule.CreateDestructibleMeshFromStaticMesh(Object->GetOuter(), Object, NAME_None, Object->GetFlags(), ErrorMsg);
			if ( DestructibleMesh )
			{
				FAssetEditorManager::Get().OpenEditorForAsset(DestructibleMesh);
				Assets.Add(DestructibleMesh);
			}
			else if ( !ErrorMsg.IsEmpty() )
			{
				FNotificationInfo ErrorNotification( ErrorMsg );
				FSlateNotificationManager::Get().AddNotification(ErrorNotification);
			}
		}
	}
	if ( Assets.Num() > 0 )
	{
		FAssetTools::Get().SyncBrowserToAssets(Assets);
	}
}

void FAssetTypeActions_StaticMesh::ExecuteSaveGeneratedLODsInPackage(TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	for (auto StaticMeshIt = Objects.CreateConstIterator(); StaticMeshIt; ++StaticMeshIt)
	{
		auto StaticMesh = (*StaticMeshIt).Get();
		if (StaticMesh)
		{
			StaticMesh->GenerateLodsInPackage();
		}
	}
}

#undef LOCTEXT_NAMESPACE
