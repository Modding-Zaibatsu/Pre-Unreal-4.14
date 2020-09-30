// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EditorTestsPrivatePCH.h"

#include "Tests/AutomationTestSettings.h"
#include "AutomationEditorCommon.h"
#include "AutomationCommon.h"
#include "ShaderCompiler.h"
#include "AssetEditorManager.h"
#include "AssetToolsModule.h"
#include "JsonObjectConverter.h"
#include "AssetRegistryModule.h"
#include "ObjectTools.h"
#include "StaticMeshResources.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

#include "FbxMeshUtils.h"
#include "FbxAutomationCommon.h"

//////////////////////////////////////////////////////////////////////////

/**
* FFbxImportAssetsAutomationTest
* Test that attempts to import .fbx files and verify that the result match the expectation (import options and result expectation are in a .json file) within the unit test directory in a sub-folder
* specified in the engine.ini file "AutomationTesting->FbxImportTestPath". Cannot be run in a commandlet
* as it executes code that routes through Slate UI.
*/
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FFbxImportAssetsAutomationTest, "Editor.Import.Fbx", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::EngineFilter))

/**
* Requests a enumeration of all sample assets to import
*/
void FFbxImportAssetsAutomationTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	FString ImportTestDirectory;
	check(GConfig);
	GConfig->GetString(TEXT("AutomationTesting.FbxImport"), TEXT("FbxImportTestPath"), ImportTestDirectory, GEngineIni);


	// Find all files in the GenericImport directory
	TArray<FString> FilesInDirectory;
	IFileManager::Get().FindFilesRecursive(FilesInDirectory, *ImportTestDirectory, TEXT("*.*"), true, false);

	// Scan all the found files, use only .fbx file
	for (TArray<FString>::TConstIterator FileIter(FilesInDirectory); FileIter; ++FileIter)
	{
		FString Filename(*FileIter);
		FString Ext = FPaths::GetExtension(Filename, true);
		if (Ext.Compare(TEXT(".fbx"), ESearchCase::IgnoreCase) == 0)
		{
			FString FileString = *FileIter;
			FString FileTestName = FPaths::GetBaseFilename(Filename);
			if (FileTestName.Len() > 6)
			{
				FString FileEndSuffixe = FileTestName.RightChop(FileTestName.Find(TEXT("_lod"), ESearchCase::IgnoreCase, ESearchDir::FromEnd));
				FString LodNumber = FileEndSuffixe.RightChop(4);
				FString LodBaseSuffixe = FileEndSuffixe.LeftChop(2);
				if (LodBaseSuffixe.Compare(TEXT("_lod"), ESearchCase::IgnoreCase) == 0)
				{
					if (LodNumber.Compare(TEXT("00")) != 0)
					{
						//Don't add lodmodel has test
						continue;
					}
				}
			}
			OutBeautifiedNames.Add(FileTestName);
			OutTestCommands.Add(FileString);
		}
	}
}

FString GetFormatedMessageErrorInTestData(FString FileName, FString TestPlanName, FString ExpectedResultName, int32 ExpectedResultIndex)
{
	return FString::Printf(TEXT("%s->%s: Error in the test data, %s[%d]"),
		*FileName, *TestPlanName, *ExpectedResultName, ExpectedResultIndex);
}

FString GetFormatedMessageErrorInExpectedResult(FString FileName, FString TestPlanName, FString ExpectedResultName, int32 ExpectedResultIndex)
{
	return FString::Printf(TEXT("%s->%s: Wrong Expected Result, %s[%d] dont match expected data"),
		*FileName, *TestPlanName, *ExpectedResultName, ExpectedResultIndex);
}

/**
* Execute the generic import test
*
* @param Parameters - Should specify the asset to import
* @return	TRUE if the test was successful, FALSE otherwise
*/
bool FFbxImportAssetsAutomationTest::RunTest(const FString& Parameters)
{
	int32 CurErrorIndex = 0, CurWarningIndex = 0, CurLogItemIndex = 0;

	TArray<FString> CurFileToImport;
	CurFileToImport.Add(*Parameters);
	FString CleanFilename = FPaths::GetCleanFilename(CurFileToImport[0]);
	FString BaseFilename = FPaths::GetBaseFilename(CurFileToImport[0]);
	FString Ext = FPaths::GetExtension(CurFileToImport[0], true);
	FString FileOptionAndResult = CurFileToImport[0];
	if(!FileOptionAndResult.RemoveFromEnd(*Ext))
	{
		ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s: Cannot find the information file (.json)"), *CleanFilename));
		return false;
	}
	FileOptionAndResult += TEXT(".json");
	
	TArray<UFbxTestPlan*> TestPlanArray;
	
	if (!IFileManager::Get().FileExists(*FileOptionAndResult))
	{
		ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s: Cannot find the information file (.json)."), *CleanFilename));
		return false;
	}

	FString PackagePath;
	check(GConfig);
	GConfig->GetString(TEXT("AutomationTesting.FbxImport"), TEXT("FbxImportTestPackagePath"), PackagePath, GEngineIni);
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	
	{
		TArray<FAssetData> AssetsToDelete;
		AssetRegistryModule.Get().GetAssetsByPath(FName(*PackagePath), AssetsToDelete, true);
		ObjectTools::DeleteAssets(AssetsToDelete, false);
	}

	//Add a folder with the file name
	FString ImportAssetPath = PackagePath + TEXT("/") + BaseFilename;
	//Read the fbx options from the .json file and fill the ImportUI
	FbxAutomationTestsAPI::ReadFbxOptions(FileOptionAndResult, TestPlanArray);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	bool CurTestSuccessful = (TestPlanArray.Num() > 0);
	TArray<UObject*> GlobalImportedObjects;
	for (UFbxTestPlan* TestPlan : TestPlanArray)
	{
		checkSlow(TestPlan->ImportUI);

		int32 WarningNum = ExecutionInfo.Warnings.Num();
		int32 ErrorNum = ExecutionInfo.Errors.Num();
		TArray<UObject*> ImportedObjects;
		switch (TestPlan->Action)
		{
			case EFBXTestPlanActionType::Import:
			{
				//Create a factory and set the options
				UFbxFactory* FbxFactory = NewObject<UFbxFactory>(UFbxFactory::StaticClass());
				FbxFactory->AddToRoot();
				FbxFactory->ImportUI = TestPlan->ImportUI;

				if (FbxFactory->ImportUI->bImportAsSkeletal)
				{
					FbxFactory->ImportUI->MeshTypeToImport = FBXIT_SkeletalMesh;
				}

				//Import the test object
				ImportedObjects = AssetToolsModule.Get().ImportAssets(CurFileToImport, ImportAssetPath, FbxFactory);

				//Add the just imported object to the global array use for reimport test
				for (UObject* ImportObject : ImportedObjects)
				{
					GlobalImportedObjects.Add(ImportObject);
				}
			}
			break;
			case EFBXTestPlanActionType::Reimport:
			{
				if (GlobalImportedObjects.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s: Cannot reimport when there is no previously imported object"), *CleanFilename));
					CurTestSuccessful = false;
					continue;
				}
				//Test expected result against the object we just reimport
				ImportedObjects.Add(GlobalImportedObjects[0]);

				if (GlobalImportedObjects[0]->IsA(UStaticMesh::StaticClass()))
				{
					UReimportFbxStaticMeshFactory* FbxStaticMeshReimportFactory = NewObject<UReimportFbxStaticMeshFactory>(UReimportFbxStaticMeshFactory::StaticClass());
					FbxStaticMeshReimportFactory->AddToRoot();
					FbxStaticMeshReimportFactory->ImportUI = TestPlan->ImportUI;

					UStaticMesh *ReimportStaticMesh = Cast<UStaticMesh>(GlobalImportedObjects[0]);
					UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(ReimportStaticMesh->AssetImportData);

					//Copy UFbxStaticMeshImportData
					ImportData->StaticMeshLODGroup = TestPlan->ImportUI->StaticMeshImportData->StaticMeshLODGroup;
					ImportData->VertexColorImportOption = TestPlan->ImportUI->StaticMeshImportData->VertexColorImportOption;
					ImportData->VertexOverrideColor = TestPlan->ImportUI->StaticMeshImportData->VertexOverrideColor;
					ImportData->bRemoveDegenerates = TestPlan->ImportUI->StaticMeshImportData->bRemoveDegenerates;
					ImportData->bBuildAdjacencyBuffer = TestPlan->ImportUI->StaticMeshImportData->bBuildAdjacencyBuffer;
					ImportData->bBuildReversedIndexBuffer = TestPlan->ImportUI->StaticMeshImportData->bBuildReversedIndexBuffer;
					ImportData->bGenerateLightmapUVs = TestPlan->ImportUI->StaticMeshImportData->bGenerateLightmapUVs;
					ImportData->bOneConvexHullPerUCX = TestPlan->ImportUI->StaticMeshImportData->bOneConvexHullPerUCX;
					ImportData->bAutoGenerateCollision = TestPlan->ImportUI->StaticMeshImportData->bAutoGenerateCollision;
					//Copy UFbxMeshImportData
					ImportData->bTransformVertexToAbsolute = TestPlan->ImportUI->StaticMeshImportData->bTransformVertexToAbsolute;
					ImportData->bBakePivotInVertex = TestPlan->ImportUI->StaticMeshImportData->bBakePivotInVertex;
					ImportData->bImportMeshLODs = TestPlan->ImportUI->StaticMeshImportData->bImportMeshLODs;
					ImportData->NormalImportMethod = TestPlan->ImportUI->StaticMeshImportData->NormalImportMethod;
					ImportData->NormalGenerationMethod = TestPlan->ImportUI->StaticMeshImportData->NormalGenerationMethod;
					//Copy UFbxAssetImportData
					ImportData->ImportTranslation = TestPlan->ImportUI->StaticMeshImportData->ImportTranslation;
					ImportData->ImportRotation = TestPlan->ImportUI->StaticMeshImportData->ImportRotation;
					ImportData->ImportUniformScale = TestPlan->ImportUI->StaticMeshImportData->ImportUniformScale;
					ImportData->bImportAsScene = TestPlan->ImportUI->StaticMeshImportData->bImportAsScene;
					ImportData->bImportMaterials = TestPlan->ImportUI->StaticMeshImportData->bImportMaterials;

					if (!FReimportManager::Instance()->Reimport(GlobalImportedObjects[0], false, false, CurFileToImport[0], FbxStaticMeshReimportFactory))
					{
						ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error when reimporting the staticmesh"), *CleanFilename, *(TestPlan->TestPlanName)));
						CurTestSuccessful = false;
						continue;
					}
				}
				else if (GlobalImportedObjects[0]->IsA(USkeletalMesh::StaticClass()))
				{
					UReimportFbxSkeletalMeshFactory* FbxSkeletalMeshReimportFactory = NewObject<UReimportFbxSkeletalMeshFactory>(UReimportFbxSkeletalMeshFactory::StaticClass());
					FbxSkeletalMeshReimportFactory->AddToRoot();
					FbxSkeletalMeshReimportFactory->ImportUI = TestPlan->ImportUI;

					USkeletalMesh *ReimportSkeletalMesh = Cast<USkeletalMesh>(GlobalImportedObjects[0]);
					UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(ReimportSkeletalMesh->AssetImportData);

					//Copy UFbxSkeletalMeshImportData
					ImportData->bImportMeshesInBoneHierarchy = TestPlan->ImportUI->SkeletalMeshImportData->bImportMeshesInBoneHierarchy;
					ImportData->bImportMorphTargets = TestPlan->ImportUI->SkeletalMeshImportData->bImportMorphTargets;
					ImportData->bKeepOverlappingVertices = TestPlan->ImportUI->SkeletalMeshImportData->bKeepOverlappingVertices;
					ImportData->bPreserveSmoothingGroups = TestPlan->ImportUI->SkeletalMeshImportData->bPreserveSmoothingGroups;
					ImportData->bUpdateSkeletonReferencePose = TestPlan->ImportUI->SkeletalMeshImportData->bUpdateSkeletonReferencePose;
					ImportData->bUseT0AsRefPose = TestPlan->ImportUI->SkeletalMeshImportData->bUseT0AsRefPose;
					//Copy UFbxMeshImportData
					ImportData->bTransformVertexToAbsolute = TestPlan->ImportUI->SkeletalMeshImportData->bTransformVertexToAbsolute;
					ImportData->bBakePivotInVertex = TestPlan->ImportUI->SkeletalMeshImportData->bBakePivotInVertex;
					ImportData->bImportMeshLODs = TestPlan->ImportUI->SkeletalMeshImportData->bImportMeshLODs;
					ImportData->NormalImportMethod = TestPlan->ImportUI->SkeletalMeshImportData->NormalImportMethod;
					ImportData->NormalGenerationMethod = TestPlan->ImportUI->SkeletalMeshImportData->NormalGenerationMethod;
					//Copy UFbxAssetImportData
					ImportData->ImportTranslation = TestPlan->ImportUI->SkeletalMeshImportData->ImportTranslation;
					ImportData->ImportRotation = TestPlan->ImportUI->SkeletalMeshImportData->ImportRotation;
					ImportData->ImportUniformScale = TestPlan->ImportUI->SkeletalMeshImportData->ImportUniformScale;
					ImportData->bImportAsScene = TestPlan->ImportUI->SkeletalMeshImportData->bImportAsScene;
					ImportData->bImportMaterials = TestPlan->ImportUI->SkeletalMeshImportData->bImportMaterials;

					if (!FReimportManager::Instance()->Reimport(GlobalImportedObjects[0], false, false, CurFileToImport[0], FbxSkeletalMeshReimportFactory))
					{
						ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error when reimporting the skeletal mesh"), *CleanFilename, *(TestPlan->TestPlanName)));
						CurTestSuccessful = false;
						FbxSkeletalMeshReimportFactory->RemoveFromRoot();
						continue;
					}
					FbxSkeletalMeshReimportFactory->RemoveFromRoot();
				}
			}
			break;
			case EFBXTestPlanActionType::AddLOD:
			case EFBXTestPlanActionType::ReimportLOD:
			{
				if (GlobalImportedObjects.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s: Cannot reimport when there is no previously imported object"), *CleanFilename));
					CurTestSuccessful = false;
					continue;
				}

				//Test expected result against the object we just reimport
				ImportedObjects.Add(GlobalImportedObjects[0]);

				FString LodIndexString = FString::FromInt(TestPlan->LodIndex);
				if (TestPlan->LodIndex < 10)
				{
					LodIndexString = TEXT("0") + LodIndexString;
				}
				LodIndexString = TEXT("_lod") + LodIndexString;
				FString BaseLODFile = CurFileToImport[0];
				FString LodFile = BaseLODFile.Replace(TEXT("_lod00"), *LodIndexString);
				if (!FPaths::FileExists(LodFile))
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s: Cannot Add Lod because file %s do not exist on disk!"), *LodFile));
					CurTestSuccessful = false;
					continue;
				}

				if (GlobalImportedObjects[0]->IsA(UStaticMesh::StaticClass()))
				{
					UStaticMesh *ExistingStaticMesh = Cast<UStaticMesh>(GlobalImportedObjects[0]);
					UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(ExistingStaticMesh->AssetImportData);

					//Copy UFbxStaticMeshImportData
					ImportData->StaticMeshLODGroup = TestPlan->ImportUI->StaticMeshImportData->StaticMeshLODGroup;
					ImportData->VertexColorImportOption = TestPlan->ImportUI->StaticMeshImportData->VertexColorImportOption;
					ImportData->VertexOverrideColor = TestPlan->ImportUI->StaticMeshImportData->VertexOverrideColor;
					ImportData->bRemoveDegenerates = TestPlan->ImportUI->StaticMeshImportData->bRemoveDegenerates;
					ImportData->bBuildAdjacencyBuffer = TestPlan->ImportUI->StaticMeshImportData->bBuildAdjacencyBuffer;
					ImportData->bBuildReversedIndexBuffer = TestPlan->ImportUI->StaticMeshImportData->bBuildReversedIndexBuffer;
					ImportData->bGenerateLightmapUVs = TestPlan->ImportUI->StaticMeshImportData->bGenerateLightmapUVs;
					ImportData->bOneConvexHullPerUCX = TestPlan->ImportUI->StaticMeshImportData->bOneConvexHullPerUCX;
					ImportData->bAutoGenerateCollision = TestPlan->ImportUI->StaticMeshImportData->bAutoGenerateCollision;
					//Copy UFbxMeshImportData
					ImportData->bTransformVertexToAbsolute = TestPlan->ImportUI->StaticMeshImportData->bTransformVertexToAbsolute;
					ImportData->bBakePivotInVertex = TestPlan->ImportUI->StaticMeshImportData->bBakePivotInVertex;
					ImportData->bImportMeshLODs = TestPlan->ImportUI->StaticMeshImportData->bImportMeshLODs;
					ImportData->NormalImportMethod = TestPlan->ImportUI->StaticMeshImportData->NormalImportMethod;
					ImportData->NormalGenerationMethod = TestPlan->ImportUI->StaticMeshImportData->NormalGenerationMethod;
					//Copy UFbxAssetImportData
					ImportData->ImportTranslation = TestPlan->ImportUI->StaticMeshImportData->ImportTranslation;
					ImportData->ImportRotation = TestPlan->ImportUI->StaticMeshImportData->ImportRotation;
					ImportData->ImportUniformScale = TestPlan->ImportUI->StaticMeshImportData->ImportUniformScale;
					ImportData->bImportAsScene = TestPlan->ImportUI->StaticMeshImportData->bImportAsScene;
					ImportData->bImportMaterials = TestPlan->ImportUI->StaticMeshImportData->bImportMaterials;

					
					FbxMeshUtils::ImportStaticMeshLOD(ExistingStaticMesh, LodFile, TestPlan->LodIndex);
				}
				else if (GlobalImportedObjects[0]->IsA(USkeletalMesh::StaticClass()))
				{
					USkeletalMesh *ExistingSkeletalMesh = Cast<USkeletalMesh>(GlobalImportedObjects[0]);
					UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(ExistingSkeletalMesh->AssetImportData);

					//Copy UFbxSkeletalMeshImportData
					ImportData->bImportMeshesInBoneHierarchy = TestPlan->ImportUI->SkeletalMeshImportData->bImportMeshesInBoneHierarchy;
					ImportData->bImportMorphTargets = TestPlan->ImportUI->SkeletalMeshImportData->bImportMorphTargets;
					ImportData->bKeepOverlappingVertices = TestPlan->ImportUI->SkeletalMeshImportData->bKeepOverlappingVertices;
					ImportData->bPreserveSmoothingGroups = TestPlan->ImportUI->SkeletalMeshImportData->bPreserveSmoothingGroups;
					ImportData->bUpdateSkeletonReferencePose = TestPlan->ImportUI->SkeletalMeshImportData->bUpdateSkeletonReferencePose;
					ImportData->bUseT0AsRefPose = TestPlan->ImportUI->SkeletalMeshImportData->bUseT0AsRefPose;
					//Copy UFbxMeshImportData
					ImportData->bTransformVertexToAbsolute = TestPlan->ImportUI->SkeletalMeshImportData->bTransformVertexToAbsolute;
					ImportData->bBakePivotInVertex = TestPlan->ImportUI->SkeletalMeshImportData->bBakePivotInVertex;
					ImportData->bImportMeshLODs = TestPlan->ImportUI->SkeletalMeshImportData->bImportMeshLODs;
					ImportData->NormalImportMethod = TestPlan->ImportUI->SkeletalMeshImportData->NormalImportMethod;
					ImportData->NormalGenerationMethod = TestPlan->ImportUI->SkeletalMeshImportData->NormalGenerationMethod;
					//Copy UFbxAssetImportData
					ImportData->ImportTranslation = TestPlan->ImportUI->SkeletalMeshImportData->ImportTranslation;
					ImportData->ImportRotation = TestPlan->ImportUI->SkeletalMeshImportData->ImportRotation;
					ImportData->ImportUniformScale = TestPlan->ImportUI->SkeletalMeshImportData->ImportUniformScale;
					ImportData->bImportAsScene = TestPlan->ImportUI->SkeletalMeshImportData->bImportAsScene;
					ImportData->bImportMaterials = TestPlan->ImportUI->SkeletalMeshImportData->bImportMaterials;

					FbxMeshUtils::ImportSkeletalMeshLOD(ExistingSkeletalMesh, LodFile, TestPlan->LodIndex);
				}
			}
			break;
		}

		//Garbage collect test options
		TestPlan->ImportUI->StaticMeshImportData->RemoveFromRoot();
		TestPlan->ImportUI->SkeletalMeshImportData->RemoveFromRoot();
		TestPlan->ImportUI->AnimSequenceImportData->RemoveFromRoot();
		TestPlan->ImportUI->TextureImportData->RemoveFromRoot();
		TestPlan->ImportUI->RemoveFromRoot();
		TestPlan->ImportUI = nullptr;

		WarningNum = ExecutionInfo.Warnings.Num() - WarningNum;
		ErrorNum = ExecutionInfo.Errors.Num() - ErrorNum;
		int32 ExpectedResultIndex = 0;
		for (const FFbxTestPlanExpectedResult &ExpectedResult : TestPlan->ExpectedResult)
		{
			switch (ExpectedResult.ExpectedPresetsType)
			{
			case Error_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 1 integer data (Expected Error number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Error_Number"), ExpectedResultIndex)));
					break;
				}
				if (ErrorNum != ExpectedResult.ExpectedPresetsDataInteger[0])
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d errors but expected %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Error_Number"), ExpectedResultIndex), ErrorNum, ExpectedResult.ExpectedPresetsDataInteger[0]));
				}
			}
			break;
			case Warning_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 1 integer data (Expected Warning number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Warning_Number"), ExpectedResultIndex)));
					break;
				}
				if (WarningNum != ExpectedResult.ExpectedPresetsDataInteger[0])
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d warnings but expected %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Warning_Number"), ExpectedResultIndex), WarningNum, ExpectedResult.ExpectedPresetsDataInteger[0]));
				}
			}
			break;
			case Created_Staticmesh_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 1 integer data (Expected Static Mesh number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Created_Staticmesh_Number"), ExpectedResultIndex)));
					break;
				}
				int32 StaticMeshImported = 0;
				for (UObject *ImportObject : ImportedObjects)
				{
					if (ImportObject->IsA(UStaticMesh::StaticClass()))
					{
						StaticMeshImported++;
					}
				}
				if (StaticMeshImported != ExpectedResult.ExpectedPresetsDataInteger[0])
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d staticmeshes created but expected %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Created_Staticmesh_Number"), ExpectedResultIndex), StaticMeshImported, ExpectedResult.ExpectedPresetsDataInteger[0]));
				}
			}
			break;
			case Created_Skeletalmesh_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 1 integer data (Expected Skeletal Mesh number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Created_Skeletalmesh_Number"), ExpectedResultIndex)));
					break;
				}
				int32 SkeletalMeshImported = 0;
				for (UObject *ImportObject : ImportedObjects)
				{
					if (ImportObject->IsA(USkeletalMesh::StaticClass()))
					{
						SkeletalMeshImported++;
					}
				}
				if (SkeletalMeshImported != ExpectedResult.ExpectedPresetsDataInteger[0])
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d skeletalmeshes created but expected %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Created_Skeletalmesh_Number"), ExpectedResultIndex), SkeletalMeshImported, ExpectedResult.ExpectedPresetsDataInteger[0]));
				}
			}
			break;
			case Materials_Created_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 1 integer data (Expected Material number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Materials_Created_Number"), ExpectedResultIndex)));
					break;
				}
				TArray<FAssetData> CreatedAssets;
				AssetRegistryModule.Get().GetAssetsByPath(FName(*PackagePath), CreatedAssets, true);
				int32 MaterialNumber = 0;
				for (FAssetData AssetData : CreatedAssets)
				{
					if (AssetData.AssetClass == UMaterial::StaticClass()->GetFName())
					{
						MaterialNumber++;
					}
				}
				if (MaterialNumber != ExpectedResult.ExpectedPresetsDataInteger[0])
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d materials created but expected %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Materials_Created_Number"), ExpectedResultIndex), MaterialNumber, ExpectedResult.ExpectedPresetsDataInteger[0]));
				}
			}
			break;
			case Vertex_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 1 integer data (Expected Vertex number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Vertex_Number"), ExpectedResultIndex)));
					break;
				}
				int32 GlobalVertexNumber = 0;
				if(ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(UStaticMesh::StaticClass()))
					{
						UStaticMesh *StaticMesh = Cast<UStaticMesh>(Object);
						for (int32 LodIndex = 0; LodIndex < StaticMesh->GetNumLODs(); ++LodIndex)
						{
							GlobalVertexNumber += StaticMesh->GetNumVertices(LodIndex);
						}
					}
					else if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *SkeletalMesh = Cast<USkeletalMesh>(Object);
						for (int32 LodIndex = 0; LodIndex < SkeletalMesh->GetResourceForRendering()->LODModels.Num(); ++LodIndex)
						{
							GlobalVertexNumber += SkeletalMesh->GetResourceForRendering()->LODModels[LodIndex].NumVertices;
						}
					}
				}
				if (GlobalVertexNumber != ExpectedResult.ExpectedPresetsDataInteger[0])
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d vertices but expected %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Vertex_Number"), ExpectedResultIndex), GlobalVertexNumber, ExpectedResult.ExpectedPresetsDataInteger[0]));
				}
			}
			break;
			case Lod_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 1 integer data (Expected LOD number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Lod_Number"), ExpectedResultIndex)));
					break;
				}
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					int32 LodNumber = 0;
					if (Object->IsA(UStaticMesh::StaticClass()))
					{
						UStaticMesh *StaticMesh = Cast<UStaticMesh>(Object);
						LodNumber = StaticMesh->GetNumLODs();
					}
					else if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *SkeletalMesh = Cast<USkeletalMesh>(Object);
						LodNumber = SkeletalMesh->GetResourceForRendering()->LODModels.Num();
					}
					if (LodNumber != ExpectedResult.ExpectedPresetsDataInteger[0])
					{
						ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d LODs but expected %d]"),
							*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Lod_Number"), ExpectedResultIndex), LodNumber, ExpectedResult.ExpectedPresetsDataInteger[0]));
					}
				}
				
			}
			break;
			case Vertex_Number_Lod:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 2)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 2 integer data (LOD index and Expected Vertex number for this LOD)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Vertex_Number_Lod"), ExpectedResultIndex)));
					break;
				}
				int32 LodIndex = ExpectedResult.ExpectedPresetsDataInteger[0];
				int32 GlobalVertexNumber = 0;
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(UStaticMesh::StaticClass()))
					{
						UStaticMesh *StaticMesh = Cast<UStaticMesh>(Object);
						if(LodIndex < StaticMesh->GetNumLODs())
						{
							GlobalVertexNumber = StaticMesh->GetNumVertices(LodIndex);
						}
					}
					else if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *SkeletalMesh = Cast<USkeletalMesh>(Object);
						if (LodIndex < SkeletalMesh->GetResourceForRendering()->LODModels.Num())
						{
							GlobalVertexNumber = SkeletalMesh->GetResourceForRendering()->LODModels[LodIndex].NumVertices;
						}
					}
				}
				if (GlobalVertexNumber != ExpectedResult.ExpectedPresetsDataInteger[1])
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d vertices but expected %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Vertex_Number_Lod"), ExpectedResultIndex), GlobalVertexNumber, ExpectedResult.ExpectedPresetsDataInteger[1]));
				}
			}
			break;
			case Mesh_Materials_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 1 integer data (Expected Material number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Mesh_Materials_Number"), ExpectedResultIndex)));
					break;
				}
				int32 MaterialIndexNumber = -1;
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(UStaticMesh::StaticClass()))
					{
						UStaticMesh *Mesh = Cast<UStaticMesh>(Object);
						MaterialIndexNumber = Mesh->StaticMaterials.Num();
					}
					else if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *Mesh = Cast<USkeletalMesh>(Object);
						MaterialIndexNumber = Mesh->Materials.Num();
					}
				}
				if (MaterialIndexNumber != ExpectedResult.ExpectedPresetsDataInteger[0])
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d materials indexes but expected %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Mesh_Materials_Number"), ExpectedResultIndex), MaterialIndexNumber, ExpectedResult.ExpectedPresetsDataInteger[0]));
				}
			}
			break;
			case Mesh_LOD_Section_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 2)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 2 integer data (LOD index and Expected sections number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Mesh_LOD_Section_Number"), ExpectedResultIndex)));
					break;
				}
				int32 SectionNumber = -1;
				int32 LODIndex = ExpectedResult.ExpectedPresetsDataInteger[0];
				bool BadLodIndex = false;
				int32 LODNumber = 0;
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(UStaticMesh::StaticClass()))
					{
						UStaticMesh *Mesh = Cast<UStaticMesh>(Object);
						LODNumber = Mesh->GetNumLODs();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							SectionNumber = Mesh->RenderData->LODResources[LODIndex].Sections.Num();
						}
					}
					else if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *Mesh = Cast<USkeletalMesh>(Object);
						LODNumber = Mesh->GetResourceForRendering()->LODModels.Num();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							SectionNumber = Mesh->GetResourceForRendering()->LODModels[LODIndex].Sections.Num();
						}
					}
				}
				if (BadLodIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error in the test data, Mesh_LOD_Section_Number LOD index [%d] is invalid. Expect LODIndex between 0 and %d which is the mesh LOD number"),
						*CleanFilename, *(TestPlan->TestPlanName), LODIndex, LODNumber));
				}
				else if (SectionNumber != ExpectedResult.ExpectedPresetsDataInteger[1])
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [LOD %d contain %d sections but expected %d section]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Mesh_LOD_Section_Number"), ExpectedResultIndex), LODIndex, SectionNumber, ExpectedResult.ExpectedPresetsDataInteger[1]));
				}
			}
			break;
			case Mesh_LOD_Section_Vertex_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 3)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 3 integer data (LOD index, section index and Expected vertex number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Mesh_LOD_Section_Vertex_Number"), ExpectedResultIndex)));
					break;
				}
				int32 LODIndex = ExpectedResult.ExpectedPresetsDataInteger[0];
				int32 SectionIndex = ExpectedResult.ExpectedPresetsDataInteger[1];
				int32 ExpectedVertexNumber = ExpectedResult.ExpectedPresetsDataInteger[2];
				int32 LODNumber = 0;
				int32 SectionNumber = 0;
				bool BadLodIndex = false;
				bool BadSectionIndex = false;
				int32 SectionVertexNumber = 0;
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(UStaticMesh::StaticClass()))
					{
						UStaticMesh *Mesh = Cast<UStaticMesh>(Object);
						LODNumber = Mesh->GetNumLODs();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							SectionNumber = Mesh->RenderData->LODResources[LODIndex].Sections.Num();
							if (SectionIndex < 0 || SectionIndex >= SectionNumber)
							{
								BadSectionIndex = true;
							}
							else
							{
								SectionVertexNumber = Mesh->RenderData->LODResources[LODIndex].Sections[SectionIndex].NumTriangles * 3;
							}
						}
					}
					else if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *Mesh = Cast<USkeletalMesh>(Object);
						LODNumber = Mesh->GetResourceForRendering()->LODModels.Num();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							SectionNumber = Mesh->GetResourceForRendering()->LODModels[LODIndex].Sections.Num();
							if (SectionIndex < 0 || SectionIndex >= SectionNumber)
							{
								BadSectionIndex = true;
							}
							else
							{
								SectionVertexNumber = Mesh->GetResourceForRendering()->LODModels[LODIndex].Sections[SectionIndex].GetNumVertices();
							}
						}
					}
				}
				if (BadLodIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error in the test data, Mesh_LOD_Section_Vertex_Number LOD index [%d] is invalid. Expect LODIndex between 0 and %d which is the mesh LOD number"),
						*CleanFilename, *(TestPlan->TestPlanName), LODIndex, LODNumber));
				}
				else if (BadSectionIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error in the test data, Mesh_LOD_Section_Vertex_Number Section index [%d] is invalid. Expect Section Index between 0 and %d which is the mesh LOD section number"),
						*CleanFilename, *(TestPlan->TestPlanName), LODIndex, SectionNumber));
				}
				else if (SectionVertexNumber != ExpectedVertexNumber)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [LOD index %d Section index %d contain %d vertex but expected %d vertex]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Mesh_LOD_Section_Vertex_Number"), ExpectedResultIndex), LODIndex, SectionIndex, SectionVertexNumber, ExpectedVertexNumber));
				}
			}
			break;
			case Mesh_LOD_Section_Triangle_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 3)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 3 integer data (LOD index, section index and Expected triangle number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Mesh_LOD_Section_Triangle_Number"), ExpectedResultIndex)));
					break;
				}
				int32 LODIndex = ExpectedResult.ExpectedPresetsDataInteger[0];
				int32 SectionIndex = ExpectedResult.ExpectedPresetsDataInteger[1];
				int32 ExpectedTriangleNumber = ExpectedResult.ExpectedPresetsDataInteger[2];
				int32 LODNumber = 0;
				int32 SectionNumber = 0;
				bool BadLodIndex = false;
				bool BadSectionIndex = false;
				int32 SectionTriangleNumber = 0;
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(UStaticMesh::StaticClass()))
					{
						UStaticMesh *Mesh = Cast<UStaticMesh>(Object);
						LODNumber = Mesh->GetNumLODs();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							SectionNumber = Mesh->RenderData->LODResources[LODIndex].Sections.Num();
							if (SectionIndex < 0 || SectionIndex >= SectionNumber)
							{
								BadSectionIndex = true;
							}
							else
							{
								SectionTriangleNumber = Mesh->RenderData->LODResources[LODIndex].Sections[SectionIndex].NumTriangles;
							}
						}
					}
					else if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *Mesh = Cast<USkeletalMesh>(Object);
						LODNumber = Mesh->GetResourceForRendering()->LODModels.Num();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							SectionNumber = Mesh->GetResourceForRendering()->LODModels[LODIndex].Sections.Num();
							if (SectionIndex < 0 || SectionIndex >= SectionNumber)
							{
								BadSectionIndex = true;
							}
							else
							{
								SectionTriangleNumber = Mesh->GetResourceForRendering()->LODModels[LODIndex].Sections[SectionIndex].NumTriangles;
							}
						}
					}
				}
				if (BadLodIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error in the test data, Mesh_LOD_Section_Triangle_Number LOD index [%d] is invalid. Expect LODIndex between 0 and %d which is the mesh LOD number"),
						*CleanFilename, *(TestPlan->TestPlanName), LODIndex, LODNumber));
				}
				else if (BadSectionIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error in the test data, Mesh_LOD_Section_Triangle_Number Section index [%d] is invalid. Expect Section Index between 0 and %d which is the mesh LOD section number"),
						*CleanFilename, *(TestPlan->TestPlanName), LODIndex, SectionNumber));
				}
				else if (SectionTriangleNumber != ExpectedTriangleNumber)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [LOD index %d Section index %d contain %d triangle but expected %d triangle]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Mesh_LOD_Section_Triangle_Number"), ExpectedResultIndex), LODIndex, SectionIndex, SectionTriangleNumber, ExpectedTriangleNumber));
				}
			}
			break;
			case Mesh_LOD_Section_Material_Name:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 2 || ExpectedResult.ExpectedPresetsDataString.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 2 integer data and 1 string(LOD index, section index and Expected material name)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Mesh_LOD_Section_Material_Name"), ExpectedResultIndex)));
					break;
				}
				int32 LODIndex = ExpectedResult.ExpectedPresetsDataInteger[0];
				int32 SectionIndex = ExpectedResult.ExpectedPresetsDataInteger[1];
				FString ExpectedMaterialName = ExpectedResult.ExpectedPresetsDataString[0];
				int32 LODNumber = 0;
				int32 SectionNumber = 0;
				bool BadLodIndex = false;
				bool BadSectionIndex = false;
				FString MaterialName;
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(UStaticMesh::StaticClass()))
					{
						UStaticMesh *Mesh = Cast<UStaticMesh>(Object);
						LODNumber = Mesh->GetNumLODs();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							SectionNumber = Mesh->RenderData->LODResources[LODIndex].Sections.Num();
							if (SectionIndex < 0 || SectionIndex >= SectionNumber)
							{
								BadSectionIndex = true;
							}
							else
							{
								int32 MaterialIndex = Mesh->RenderData->LODResources[LODIndex].Sections[SectionIndex].MaterialIndex;
								if (MaterialIndex >= 0 && MaterialIndex < Mesh->StaticMaterials.Num() && Mesh->StaticMaterials[MaterialIndex].MaterialInterface != nullptr)
								{
									MaterialName = Mesh->StaticMaterials[MaterialIndex].MaterialInterface->GetName();
								}
							}
						}
					}
					else if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *Mesh = Cast<USkeletalMesh>(Object);
						LODNumber = Mesh->GetResourceForRendering()->LODModels.Num();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							SectionNumber = Mesh->GetResourceForRendering()->LODModels[LODIndex].Sections.Num();
							if (SectionIndex < 0 || SectionIndex >= SectionNumber)
							{
								BadSectionIndex = true;
							}
							else
							{
								int32 MaterialIndex = Mesh->GetResourceForRendering()->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex;
								if (MaterialIndex >= 0 && MaterialIndex < Mesh->Materials.Num())
								{
									MaterialName = Mesh->Materials[MaterialIndex].MaterialInterface->GetName();
								}
							}
						}
					}
				}
				if (BadLodIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error in the test data, Mesh_LOD_Section_Material_Name LOD index [%d] is invalid. Expect LODIndex between 0 and %d which is the mesh LOD number"),
						*CleanFilename, *(TestPlan->TestPlanName), LODIndex, LODNumber));
				}
				else if (BadSectionIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error in the test data, Mesh_LOD_Section_Material_Name Section index [%d] is invalid. Expect Section Index between 0 and %d which is the mesh LOD section number"),
						*CleanFilename, *(TestPlan->TestPlanName), LODIndex, SectionNumber));
				}
				else if (MaterialName.Compare(ExpectedMaterialName) != 0)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [LOD index %d Section index %d contain material name (%s) but expected name (%s)]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Mesh_LOD_Section_Material_Name"), ExpectedResultIndex), LODIndex, SectionIndex, *MaterialName, *ExpectedMaterialName));
				}
			}
			break;
			case Mesh_LOD_Section_Material_Index:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 2 || ExpectedResult.ExpectedPresetsDataString.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 3 integer data (LOD index, section index and Expected material index)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Mesh_LOD_Section_Material_Index"), ExpectedResultIndex)));
					break;
				}
				int32 LODIndex = ExpectedResult.ExpectedPresetsDataInteger[0];
				int32 SectionIndex = ExpectedResult.ExpectedPresetsDataInteger[1];
				int32 ExpectedMaterialIndex = ExpectedResult.ExpectedPresetsDataInteger[2];
				int32 LODNumber = 0;
				int32 SectionNumber = 0;
				bool BadLodIndex = false;
				bool BadSectionIndex = false;
				int32 MaterialIndex = 0;
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(UStaticMesh::StaticClass()))
					{
						UStaticMesh *Mesh = Cast<UStaticMesh>(Object);
						LODNumber = Mesh->GetNumLODs();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							SectionNumber = Mesh->RenderData->LODResources[LODIndex].Sections.Num();
							if (SectionIndex < 0 || SectionIndex >= SectionNumber)
							{
								BadSectionIndex = true;
							}
							else
							{
								MaterialIndex = Mesh->RenderData->LODResources[LODIndex].Sections[SectionIndex].MaterialIndex;
							}
						}
					}
					else if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *Mesh = Cast<USkeletalMesh>(Object);
						LODNumber = Mesh->GetResourceForRendering()->LODModels.Num();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							SectionNumber = Mesh->GetResourceForRendering()->LODModels[LODIndex].Sections.Num();
							if (SectionIndex < 0 || SectionIndex >= SectionNumber)
							{
								BadSectionIndex = true;
							}
							else
							{
								MaterialIndex = Mesh->GetResourceForRendering()->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex;
							}
						}
					}
				}
				if (BadLodIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error in the test data, Mesh_LOD_Section_Material_Index LOD index [%d] is invalid. Expect LODIndex between 0 and %d which is the mesh LOD number"),
						*CleanFilename, *(TestPlan->TestPlanName), LODIndex, LODNumber));
				}
				else if (BadSectionIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error in the test data, Mesh_LOD_Section_Material_Index Section index [%d] is invalid. Expect Section Index between 0 and %d which is the mesh LOD section number"),
						*CleanFilename, *(TestPlan->TestPlanName), LODIndex, SectionNumber));
				}
				else if (MaterialIndex != ExpectedMaterialIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [LOD index %d Section index %d contain material index %d but expected index %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Mesh_LOD_Section_Material_Index"), ExpectedResultIndex), LODIndex, SectionIndex, MaterialIndex, ExpectedMaterialIndex));
				}
			}
			break;
			case LOD_UV_Channel_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 2)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 2 integer data (LOD index and Expected UV Channel number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("LOD_UV_Channel_Number"), ExpectedResultIndex)));
					break;
				}
				int32 UVChannelNumber = -1;
				int32 LODIndex = ExpectedResult.ExpectedPresetsDataInteger[0];
				int32 ExpectedUVNumber = ExpectedResult.ExpectedPresetsDataInteger[1];
				int32 LODNumber = -1;
				bool BadLodIndex = false;
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(UStaticMesh::StaticClass()))
					{
						UStaticMesh *Mesh = Cast<UStaticMesh>(Object);
						LODNumber = Mesh->GetNumLODs();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							UVChannelNumber = Mesh->RenderData->LODResources[LODIndex].GetNumTexCoords();
						}
					}
					else if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *Mesh = Cast<USkeletalMesh>(Object);
						LODNumber = Mesh->GetResourceForRendering()->LODModels.Num();
						if (LODIndex < 0 || LODIndex >= LODNumber)
						{
							BadLodIndex = true;
						}
						else
						{
							UVChannelNumber = Mesh->GetResourceForRendering()->LODModels[LODIndex].NumTexCoords;
						}
					}
				}
				if (BadLodIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Error in the test data, LOD_UV_Channel_Number LOD index [%d] is invalid. Expect LODIndex between 0 and %d which is the mesh LOD number"),
						*CleanFilename, *(TestPlan->TestPlanName), LODIndex, LODNumber));
				}
				else if (UVChannelNumber != ExpectedUVNumber)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d UVChannels but expected %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("LOD_UV_Channel_Number"), ExpectedResultIndex), UVChannelNumber, ExpectedUVNumber));
				}
			}
			break;
			case Bone_Number:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 1)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 1 integer data (Expected Bone number)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Bone_Number"), ExpectedResultIndex)));
					break;
				}
				int32 BoneNumber = -1;
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						USkeletalMesh *Mesh = Cast<USkeletalMesh>(Object);
						if (Mesh->Skeleton != nullptr)
						{
							BoneNumber = Mesh->Skeleton->GetReferenceSkeleton().GetNum();
						}
					}
				}
				if (BoneNumber != ExpectedResult.ExpectedPresetsDataInteger[0])
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [%d bones but expected %d]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Bone_Number"), ExpectedResultIndex), BoneNumber, ExpectedResult.ExpectedPresetsDataInteger[0]));
				}
			}
			break;
			case Bone_Position:
			{
				if (ExpectedResult.ExpectedPresetsDataInteger.Num() < 1 || ExpectedResult.ExpectedPresetsDataFloat.Num() < 3)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s expected result need 1 integer data and 3 float data (Bone index and expected bone position XYZ)"),
						*GetFormatedMessageErrorInTestData(CleanFilename, TestPlan->TestPlanName, TEXT("Bone_Position"), ExpectedResultIndex)));
					break;
				}
				int32 BoneIndex = ExpectedResult.ExpectedPresetsDataInteger[0];
				FVector ExpectedBonePosition(ExpectedResult.ExpectedPresetsDataFloat[0], ExpectedResult.ExpectedPresetsDataFloat[1], ExpectedResult.ExpectedPresetsDataFloat[2]);
				float Epsilon = ExpectedResult.ExpectedPresetsDataFloat.Num() == 4 ? ExpectedResult.ExpectedPresetsDataFloat[3] : FLT_EPSILON;
				FVector BoneIndexPosition(FLT_MAX);
				bool FoundSkeletalMesh = false;
				bool FoundSkeleton = false;
				bool FoundBoneIndex = false;
				int32 BoneNumber = -1;
				if (ImportedObjects.Num() > 0)
				{
					UObject *Object = ImportedObjects[0];
					if (Object->IsA(USkeletalMesh::StaticClass()))
					{
						FoundSkeletalMesh = true;
						USkeletalMesh *Mesh = Cast<USkeletalMesh>(Object);
						FoundSkeleton = true;
						BoneNumber = Mesh->RefSkeleton.GetNum();
						if (BoneIndex >= 0 && BoneIndex < BoneNumber)
						{
							FoundBoneIndex = true;
							BoneIndexPosition = Mesh->RefSkeleton.GetRefBonePose()[BoneIndex].GetLocation();
						}
					}
				}
				if (!FoundSkeletalMesh)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Wrong Expected Result, there is no skeletal mesh imported"),
						*CleanFilename, *(TestPlan->TestPlanName)));
				}
				else if (!FoundSkeleton)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Wrong Expected Result, there is no skeleton attach to this skeletal mesh"),
						*CleanFilename, *(TestPlan->TestPlanName)));
				}
				else if (!FoundBoneIndex)
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Wrong Expected Result, the bone index is not a valid index (bone index [%d] bone number[%d])"),
						*CleanFilename, *(TestPlan->TestPlanName), BoneIndex, BoneNumber));
				}
				if (!BoneIndexPosition.Equals(ExpectedBonePosition, Epsilon))
				{
					ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s [X:%f, Y:%f, Z:%f but expected X:%f, Y:%f, Z:%f]"),
						*GetFormatedMessageErrorInExpectedResult(*CleanFilename, *(TestPlan->TestPlanName), TEXT("Bone_Position"), ExpectedResultIndex), BoneIndexPosition.X, BoneIndexPosition.Y, BoneIndexPosition.Z, ExpectedBonePosition.X, ExpectedBonePosition.Y, ExpectedBonePosition.Z));
				}
			}
			break;
			default:
			{
				ExecutionInfo.Errors.Add(FString::Printf(TEXT("%s->%s: Wrong Test plan, Unknown expected result preset."),
					*CleanFilename, *(TestPlan->TestPlanName)));
			}
			break;
			};
			ExpectedResultIndex++;
		}
		if (TestPlan->bDeleteFolderAssets)
		{
			GlobalImportedObjects.Empty();
			TArray<FAssetData> AssetsToDelete;
			AssetRegistryModule.Get().GetAssetsByPath(FName(*PackagePath), AssetsToDelete, true);
			TArray<UObject*> ObjectToDelete;
			for (const FAssetData& AssetData : AssetsToDelete)
			{
				UObject *Asset = AssetData.GetAsset();
				if (Asset != nullptr)
					ObjectToDelete.Add(Asset);
			}
			ObjectTools::ForceDeleteObjects(ObjectToDelete, false);
		}
		// Any errors, warnings, or log items that are caught during this unit test aren't guaranteed to include the name of the file that generated them,
		// which can be confusing when reading results. Alleviate the issue by injecting the file name for each error, warning, or log item, where appropriate.
		for (int32 ErrorStartIndex = CurErrorIndex; ErrorStartIndex < ExecutionInfo.Errors.Num(); ++ErrorStartIndex)
		{
			ExecutionInfo.Errors[ErrorStartIndex] = FString::Printf(TEXT("%s: %s"), *CleanFilename, *ExecutionInfo.Errors[ErrorStartIndex].ToString());
		}
		for (int32 WarningStartIndex = CurWarningIndex; WarningStartIndex < ExecutionInfo.Warnings.Num(); ++WarningStartIndex)
		{
			ExecutionInfo.Warnings[WarningStartIndex] = FString::Printf(TEXT("%s: %s"), *CleanFilename, *ExecutionInfo.Warnings[WarningStartIndex]);
		}
		for (int32 LogItemStartIndex = CurLogItemIndex; LogItemStartIndex < ExecutionInfo.LogItems.Num(); ++LogItemStartIndex)
		{
			ExecutionInfo.LogItems[LogItemStartIndex] = FString::Printf(TEXT("%s: %s"), *CleanFilename, *ExecutionInfo.LogItems[LogItemStartIndex]);
		}
		CurErrorIndex = ExecutionInfo.Errors.Num();
		CurWarningIndex = ExecutionInfo.Warnings.Num();
		CurLogItemIndex = ExecutionInfo.LogItems.Num();
	}

	return CurTestSuccessful;
}
