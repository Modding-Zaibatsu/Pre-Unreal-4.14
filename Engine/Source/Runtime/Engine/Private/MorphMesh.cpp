// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MorphMesh.cpp: Unreal morph target mesh and blending implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Animation/MorphTarget.h"

//////////////////////////////////////////////////////////////////////////

UMorphTarget::UMorphTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void UMorphTarget::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );
	FStripDataFlags StripFlags( Ar );
	if( !StripFlags.IsDataStrippedForServer() )
	{
		Ar << MorphLODModels;
	}
}


void UMorphTarget::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	for (const auto& LODModel : MorphLODModels)
	{
		LODModel.GetResourceSizeEx(CumulativeResourceSize);
	}
}


//////////////////////////////////////////////////////////////////////
SIZE_T FMorphTargetLODModel::GetResourceSize() const
{
	return GetResourceSizeBytes();
}

void FMorphTargetLODModel::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
{
	CumulativeResourceSize.AddUnknownMemoryBytes(Vertices.GetAllocatedSize() + sizeof(int32));
}

SIZE_T FMorphTargetLODModel::GetResourceSizeBytes() const
{
	FResourceSizeEx ResSize;
	GetResourceSizeEx(ResSize);
	return ResSize.GetTotalMemoryBytes();
}
