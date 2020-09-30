// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"


/**
 * Dynamic rush for referencing a UMaterial.  
 *
 * Note: This brush nor the slate renderer holds a strong reference to the material.  You are responsible for maintaining the lifetime of the brush and material object.
 */
struct FSlateMaterialBrush : public FSlateBrush
{
	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InMaterial The material to use.
	 * @param InImageSize The material's dimensions.
	 */
	FSlateMaterialBrush( class UMaterialInterface& InMaterial, const FVector2D& InImageSize )
		: FSlateBrush( ESlateBrushDrawType::Image, FName(TEXT("None")), FMargin(0), ESlateBrushTileType::NoTile, ESlateBrushImageType::FullColor, InImageSize, FLinearColor::White, &InMaterial )
	{
		ResourceName = FName( *InMaterial.GetFullName() );
	}

	/** Virtual destructor. */
	virtual ~FSlateMaterialBrush( )
	{
		if(FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer().IsValid())
		{
			FSlateApplication::Get().GetRenderer()->ReleaseDynamicResource( *this );
		}
	}
}; 
