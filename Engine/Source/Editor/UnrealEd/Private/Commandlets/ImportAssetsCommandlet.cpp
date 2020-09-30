// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AutomatedAssetImportData.h"
#include "ImportAssetsCommandlet.h"
#include "AssetToolsModule.h"
#include "JsonObjectConverter.h"


static void PrintUsage()
{
	UE_LOG(LogAutomatedImport, Display, TEXT("LogAutomatedImport Usage: LogAutomatedImport {arglist}"));
	UE_LOG(LogAutomatedImport, Display, TEXT("Arglist:"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-help or -?"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tDisplays this help"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-source=\"path\""));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tThe source file to import.  This must be specified when importing a single asset\n[IGNORED when using -importparams]"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-dest=\"path/optional_asset_name\""));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tThe destination path in the project's content directory to import to with an optional asset name.\nIf the asset name is not specified the name of the source file will be used as the asset name.\nThis must be specified when importing a single asset.\n[IGNORED when using -importparams]"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-factory={factory class name}"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tForces the asset to be opened with a specific UFactory class type.  If not specified import type will be auto detected.\n[IGNORED when using -importparams]"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-importsettings=\"path to import settings json file\""));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tPath to a json file that has asset import parameters when importing multiple files. If this argument is used all other import arguments are ignored as they are specified in the json file"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-replaceexisting"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tWhether or not to replace existing assets when importing"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-nosourcecontrol"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tDisables source control.  Prevents checking out, adding files, and submitting files"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-submitdesc"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tSubmit description/comment to use checking in to source control.  If this is empty no files will be submitted"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-skipreadonly"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tIf an asset cannot be saved because it is read only, the commandlet will not clear the read only flag and will not save the file"));
}

UImportAssetsCommandlet::UImportAssetsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UImportAssetsCommandlet::ParseParams(const FString& InParams)
{
	TArray<FString> Tokens;
	TArray<FString> Params;
	TMap<FString, FString> ParamVals;

	ParseCommandLine(*InParams, Tokens, Params, ParamVals);

	const bool bEnoughParams = ParamVals.Num() > 1;
	if( Params.Contains(TEXT("?")) || Params.Contains(TEXT("help") ) )
	{
		bShowHelp = true;
	}

	bAllowSourceControl = !Params.Contains(TEXT("nosourcecontrol"));

	GlobalImportData = NewObject<UAutomatedAssetImportData>(this);

	GlobalImportData->bSkipReadOnly = Params.Contains(TEXT("skipreadonly"));

	FString SourcePathParam = ParamVals.FindRef(TEXT("source"));
	if(!SourcePathParam.IsEmpty())
	{
		GlobalImportData->Filenames.Add(SourcePathParam);
	}
	
	GlobalImportData->DestinationPath = ParamVals.FindRef(TEXT("dest"));

	GlobalImportData->FactoryName = ParamVals.FindRef(TEXT("factoryname"));

	GlobalImportData->bReplaceExisting = Params.Contains(TEXT("replaceexisting"));

	ImportSettingsPath = ParamVals.FindRef(TEXT("importsettings"));

	GlobalImportData->Initialize();

	if(ImportSettingsPath.IsEmpty() && (GlobalImportData->Filenames.Num() == 0 || GlobalImportData->DestinationPath.IsEmpty()))
	{
		UE_LOG(LogAutomatedImport, Error, TEXT("Invalid Arguments.  Missing, Source (-source), Destination (-dest), or Import settings file (-importsettings)"));
	}

	return bEnoughParams;
}

bool UImportAssetsCommandlet::ParseImportSettings(const FString& InImportSettingsFile)
{
	bool bInvalidParse = false;
	bool bSuccess = false;
	FString JsonString;
	if(FFileHelper::LoadFileToString(JsonString, *InImportSettingsFile))
	{
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
		TSharedPtr<FJsonObject> RootObject;
		if(FJsonSerializer::Deserialize(JsonReader, RootObject) && RootObject.IsValid())
		{
			const TArray< TSharedPtr<FJsonValue> > ImportGroupsJsonArray = RootObject->GetArrayField(TEXT("ImportGroups"));
			for(const TSharedPtr<FJsonValue>& ImportGroupsJson : ImportGroupsJsonArray)
			{
				const TSharedPtr<FJsonObject> ImportGroupsJsonObject = ImportGroupsJson->AsObject();
				if(ImportGroupsJsonObject.IsValid())
				{
					// All import data is based off of the global data defaults
					UAutomatedAssetImportData* Data = DuplicateObject<UAutomatedAssetImportData>(GlobalImportData, this);
					
					// Parse data from the json object
					if(FJsonObjectConverter::JsonObjectToUStruct(ImportGroupsJsonObject.ToSharedRef(), UAutomatedAssetImportData::StaticClass(), Data, 0, 0 ))
					{
						Data->Initialize();
						if(Data->IsValid())
						{
							ImportDataList.Add(Data);
						}

						UFactory* Factory = Data->Factory;
						const TSharedPtr<FJsonObject>* ImportSettingsJsonObject = nullptr;
						ImportGroupsJsonObject->TryGetObjectField(TEXT("ImportSettings"), ImportSettingsJsonObject);
						if(Factory != nullptr && ImportSettingsJsonObject)
						{
							IImportSettingsParser* ImportSettings = Factory->GetImportSettingsParser();
							if(ImportSettings)
							{
								ImportSettings->ParseFromJson(ImportSettingsJsonObject->ToSharedRef());
							}
						}
						else if(Factory == nullptr && ImportSettingsJsonObject)
						{
							UE_LOG(LogAutomatedImport, Warning, TEXT("A vaild factory name must be specfied in order to specify settings"));
						}
					}
					else
					{
						bInvalidParse = true;
					}
					
				}
				else
				{
					bInvalidParse = true;
				}
			}
		}
		else
		{
			UE_LOG(LogAutomatedImport, Error, TEXT("Json settings file was found but was invalid: %s"), *JsonReader->GetErrorMessage());
		}
	}
	else
	{
		UE_LOG(LogAutomatedImport, Error, TEXT("Import settings file %s could not be found"), *InImportSettingsFile);
	}

	return bSuccess;
}

static bool SavePackage(UPackage* Package, const FString& PackageFilename)
{
	return GEditor->SavePackage(Package, nullptr, RF_Standalone, *PackageFilename, GWarn);
}

bool UImportAssetsCommandlet::ImportAndSave(const TArray<UAutomatedAssetImportData*>& AssetImportList)
{
	bool bImportAndSaveSucceeded = true;

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	for(const UAutomatedAssetImportData* ImportData : AssetImportList)
	{
		UE_LOG(LogAutomatedImport, Log, TEXT("Importing group %s"), *ImportData->GetDisplayName() );
		TArray<UObject*> ImportedAssets = AssetToolsModule.Get().ImportAssetsAutomated(*ImportData);
		if(ImportedAssets.Num() > 0)
		{
			TArray<UPackage*> DirtyPackages;
			TArray<FSourceControlStateRef> PackageStates;

			FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);

			bool bUseSourceControl = bHasSourceControl && SourceControlProvider.IsAvailable();
			if(bUseSourceControl)
			{
				SourceControlProvider.GetState(DirtyPackages, PackageStates, EStateCacheUsage::ForceUpdate);
			}

			for(int32 PackageIndex = 0; PackageIndex < DirtyPackages.Num(); ++PackageIndex)
			{
				UPackage* PackageToSave = DirtyPackages[PackageIndex];
				FSourceControlStateRef PackageSCState = PackageStates[PackageIndex];

				FString PackageFilename = SourceControlHelpers::PackageFilename(PackageToSave);

				bool bShouldAttemptToSave = false;
				bool bShouldAttemptToAdd = false;
				if(bUseSourceControl)
				{
					bool bPackageCanBeCheckedOut = false;
					if(PackageSCState->IsCheckedOutOther())
					{
						// Cannot checkout, file is already checked out
						UE_LOG(LogAutomatedImport, Error, TEXT("%s is already checked out by someone else, can not submit!"), *PackageFilename);
						bImportAndSaveSucceeded = false;
					}
					else if(!PackageSCState->IsCurrent())
					{
						// Cannot checkout, file is not at head revision
						UE_LOG(LogAutomatedImport, Error, TEXT("%s is not at the head revision and cannot be checked out"), *PackageFilename);
						bImportAndSaveSucceeded = false;
					}
					else if(PackageSCState->CanCheckout())
					{
						const bool bWasCheckedOut = SourceControlHelpers::CheckOutFile(PackageFilename);
						bShouldAttemptToSave = bWasCheckedOut;
						if(!bWasCheckedOut)
						{
							UE_LOG(LogAutomatedImport, Error, TEXT("%s could not be checked out"), *PackageFilename);
							bImportAndSaveSucceeded = false;
						}
					}
					else
					{
						// package was not checked out by another user and is at the current head revision and could not be checked out
						// this means it should be added after save because it doesnt exist
						bShouldAttemptToSave = true;
						bShouldAttemptToAdd = true;
					}
				}
				else
				{
					bool bIsReadOnly = IFileManager::Get().IsReadOnly(*PackageFilename);
					if(bIsReadOnly && ImportData->bSkipReadOnly)
					{
						bShouldAttemptToSave = ImportData->bSkipReadOnly;
						if(bIsReadOnly)
						{
							UE_LOG(LogAutomatedImport, Error, TEXT("%s is read only and -skipreadonly was specified.  Will not save"), *PackageFilename);
							bImportAndSaveSucceeded = false;
						}
					}
					else if(bIsReadOnly)
					{
						bShouldAttemptToSave = FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*PackageFilename, false);
						if(!bShouldAttemptToSave)
						{
							UE_LOG(LogAutomatedImport, Error, TEXT("%s is read only and could not be made writable.  Will not save"), *PackageFilename);
							bImportAndSaveSucceeded = false;
						}
					}
				}

				if(bShouldAttemptToSave)
				{
					SavePackage(PackageToSave, PackageFilename);

					if(bShouldAttemptToAdd)
					{
						const bool bWasAdded = SourceControlHelpers::MarkFileForAdd(PackageFilename);
						if(!bWasAdded)
						{
							UE_LOG(LogAutomatedImport, Error, TEXT("%s could not be added to source control"), *PackageFilename);
							bImportAndSaveSucceeded = false;
						}
					}
				}
			}
		}
		else
		{
			bImportAndSaveSucceeded = false;
			UE_LOG(LogAutomatedImport, Error, TEXT("Failed to import all assets in group %s"), *ImportData->GetDisplayName());
		}
	}

	return bImportAndSaveSucceeded;
}

void UImportAssetsCommandlet::ClearDirtyPackages()
{
	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);
	for(UPackage* Package : DirtyPackages)
	{
		Package->SetDirtyFlag(false);
	}
}

int32 UImportAssetsCommandlet::Main(const FString& InParams)
{
	bool bEnoughParams = ParseParams(InParams);

	int32 Result = 0;

	if(!bEnoughParams || bShowHelp)
	{
		PrintUsage();
	}
	else
	{
		// Hack:  A huge amount of packages are marked dirty on startup.  This is normally prevented in editor but commandlets have special powers.  
		// We only want to save assets that were created or modified at import time so clear all existing ones now
		ClearDirtyPackages();

		if(bAllowSourceControl)
		{
			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
			SourceControlProvider.Init();

			bHasSourceControl = SourceControlProvider.IsEnabled();
			if(!bHasSourceControl)
			{
				UE_LOG(LogAutomatedImport, Error, TEXT("Could not connect to source control!"))
			}
		}
		else
		{
			bHasSourceControl = false;
		}


		if(!ImportSettingsPath.IsEmpty())
		{
			// Use settings file for importing assets
			ParseImportSettings(ImportSettingsPath);
		}
		else if(GlobalImportData->IsValid())
		{
			// Use single import path
			ImportDataList.Add(GlobalImportData);
		}

		if(!ImportAndSave(ImportDataList))
		{
			UE_LOG(LogAutomatedImport, Error, TEXT("Could not import all groups"));
		}
		else
		{
			Result = 0;
		}
		
	}
	return Result;
}
