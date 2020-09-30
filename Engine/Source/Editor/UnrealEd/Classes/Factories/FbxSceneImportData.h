// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FbxSceneImportFactory.h"

#include "FbxSceneImportData.generated.h"

UENUM()
enum class EFbxSceneReimportStatusFlags : uint8
{
	None = 0x00,
	Added = 0x01,
	Removed = 0x02,
	Same = 0x04,
	FoundContentBrowserAsset = 0x08,
	ReimportAsset = 0x10,
};
ENUM_CLASS_FLAGS(EFbxSceneReimportStatusFlags);


UCLASS()
class UNREALED_API UFbxSceneImportData : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
public:
	/* The path of the fbx file use for the last import */
	UPROPERTY(VisibleAnywhere, Category = ImportSettings)
	FString SourceFbxFile;

	/* The full name of the blueprint create at import */
	FString BluePrintFullName;

	/* Is the last reimport has import the hierarchy */
	bool bImportScene;

	/* Is the original import create a folder hierarchy */
	bool bCreateFolderHierarchy;

	/* Which type of hierarchy was create see */
	int32 HierarchyType;

	//The last import scene hierarchy data
	TSharedPtr<FFbxSceneInfo> SceneInfoSourceData;

	ImportOptionsNameMap NameOptionsMap;

	/** Convert this import information to JSON */
	FString ToJson() const;

	/** Attempt to parse an asset import structure from the specified json string. */
	void FromJson(FString InJsonString);

protected:
	/** Overridden serialize function to write out the underlying data as json */
	virtual void Serialize(FArchive& Ar) override;
#endif
};



