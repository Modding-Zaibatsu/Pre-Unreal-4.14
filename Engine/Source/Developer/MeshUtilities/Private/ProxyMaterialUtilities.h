// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace ProxyMaterialUtilities
{
#define TEXTURE_MACRO_BASE(a, b, c) \
	bool b##a##Texture = FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::a) && !FlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::a); \
	if ( b##a##Texture) \
	{ \
		UTexture2D* a##Texture = b##a##Texture ? FMaterialUtilities::CreateTexture(InOuter, AssetBasePath + TEXT("T_") + AssetBaseName + TEXT("_" #a), FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::a), FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::a), b, TEXTUREGROUP_HierarchicalLOD, RF_Public | RF_Standalone, c) : nullptr; \
		OutMaterial->SetTextureParameterValueEditorOnly(#a "Texture", a##Texture); \
		FStaticSwitchParameter SwitchParameter; \
		SwitchParameter.ParameterName = "Use" #a; \
		SwitchParameter.Value = true; \
		SwitchParameter.bOverride = true; \
		NewStaticParameterSet.StaticSwitchParameters.Add(SwitchParameter); \
		a##Texture->PostEditChange(); \
	} 

#define TEXTURE_MACRO_VECTOR(a, b, c) TEXTURE_MACRO_BASE(a, b, c)\
	else\
	{ \
		OutMaterial->SetVectorParameterValueEditorOnly(#a "Const", FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::a)[0]); \
	} 

#define TEXTURE_MACRO_VECTOR_LINEAR(a, b, c) TEXTURE_MACRO_BASE(a, b, c)\
	else\
	{ \
		OutMaterial->SetVectorParameterValueEditorOnly(#a "Const", FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::a)[0].ReinterpretAsLinear()); \
	} 

#define TEXTURE_MACRO_SCALAR(a, b, c) TEXTURE_MACRO_BASE(a, b, c)\
	else \
	{ \
		FLinearColor Colour = FlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::a) ? FLinearColor::FromSRGBColor( FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::a)[0]) : FLinearColor( InMaterialProxySettings.a##Constant, 0, 0, 0 ); \
		OutMaterial->SetScalarParameterValueEditorOnly(#a "Const", Colour.R ); \
	}

	static const bool CalculatePackedTextureData(const FFlattenMaterial& InMaterial, bool& bOutPackMetallic, bool& bOutPackSpecular, bool& bOutPackRoughness, int32& OutNumSamples, FIntPoint& OutSize)
	{
		// Whether or not a material property is baked down
		const bool bHasMetallic = InMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Metallic) && !InMaterial.IsPropertyConstant(EFlattenMaterialProperties::Metallic);
		const bool bHasRoughness = InMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Roughness) && !InMaterial.IsPropertyConstant(EFlattenMaterialProperties::Roughness);
		const bool bHasSpecular = InMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Specular) && !InMaterial.IsPropertyConstant(EFlattenMaterialProperties::Specular);

		// Check for same texture sizes
		bool bSameTextureSize = false;

		// Determine whether or not the properties sizes match


		const FIntPoint MetallicSize = InMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic);
		const FIntPoint SpecularSize = InMaterial.GetPropertySize(EFlattenMaterialProperties::Specular);
		const FIntPoint RoughnessSize = InMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness);

		bSameTextureSize = (MetallicSize == RoughnessSize) || (MetallicSize == SpecularSize);
		if (bSameTextureSize)
		{
			OutSize = MetallicSize;
			OutNumSamples = InMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic).Num();
		}
		else
		{
			bSameTextureSize = (RoughnessSize == SpecularSize);
			if (bSameTextureSize)
			{
				OutSize = RoughnessSize;
				OutNumSamples = InMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness).Num();
			}
		}

		// Now that we know if the data matches determine whether or not we should pack the properties
		int32 NumPacked = 0;
		if (OutNumSamples != 0)
		{
			bOutPackMetallic = bHasMetallic ? (OutNumSamples == InMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic).Num()) : false;
			NumPacked += (bOutPackMetallic) ? 1 : 0;
			bOutPackRoughness = bHasRoughness ? (OutNumSamples == InMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness).Num()) : false;
			NumPacked += (bOutPackRoughness) ? 1 : 0;
			bOutPackSpecular = bHasSpecular ? (OutNumSamples == InMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular).Num()) : false;
			NumPacked += (bOutPackSpecular) ? 1 : 0;
		}
		else
		{
			bOutPackMetallic = bOutPackRoughness = bOutPackSpecular = false;
		}

		// Need atleast two properties to pack
		return NumPacked >= 2;
	}

	static UMaterialInstanceConstant* CreateProxyMaterialInstance(UPackage* InOuter, const FMaterialProxySettings& InMaterialProxySettings, FFlattenMaterial& FlattenMaterial, const FString& AssetBasePath, const FString& AssetBaseName)
	{
		UMaterial* BaseMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EngineMaterials/BaseFlattenMaterial.BaseFlattenMaterial"), NULL, LOAD_None, NULL);
		check(BaseMaterial);

		UMaterialInstanceConstant* OutMaterial = FMaterialUtilities::CreateInstancedMaterial(BaseMaterial, InOuter, AssetBasePath + AssetBaseName, RF_Public | RF_Standalone);

		OutMaterial->BasePropertyOverrides.TwoSided = false;
		OutMaterial->BasePropertyOverrides.bOverride_TwoSided = true;
		OutMaterial->BasePropertyOverrides.DitheredLODTransition = FlattenMaterial.bDitheredLODTransition;
		OutMaterial->BasePropertyOverrides.bOverride_DitheredLODTransition = true;

		bool bPackMetallic, bPackSpecular, bPackRoughness;
		int32 NumSamples = 0;
		FIntPoint PackedSize;
		const bool bPackTextures = CalculatePackedTextureData(FlattenMaterial, bPackMetallic, bPackSpecular, bPackRoughness, NumSamples, PackedSize);

		const bool bSRGB = true;
		const bool bRGB = false;

		FStaticParameterSet NewStaticParameterSet;

		// Load textures and set switches accordingly
		if (FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse).Num() > 0 && !(FlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Diffuse) && FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse)[0] == FColor::Black))
		{
			TEXTURE_MACRO_VECTOR(Diffuse, TC_Default, bSRGB);
		}

		if (FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal).Num() > 1)
		{
			TEXTURE_MACRO_BASE(Normal, TC_Normalmap, bRGB)
		}

		// Determine whether or not specific material properties are packed together into one texture (requires at least two to match (number of samples and texture size) in order to be packed
		if (!bPackMetallic && (FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic).Num() > 0 || !InMaterialProxySettings.bMetallicMap))
		{
			TEXTURE_MACRO_SCALAR(Metallic, TC_Default, bSRGB);
		}

		if (!bPackRoughness && (FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness).Num() > 0 || !InMaterialProxySettings.bRoughnessMap))
		{
			TEXTURE_MACRO_SCALAR(Roughness, TC_Default, bSRGB);
		}

		if (!bPackSpecular && (FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular).Num() > 0 || !InMaterialProxySettings.bSpecularMap))
		{
			TEXTURE_MACRO_SCALAR(Specular, TC_Default, bSRGB);
		}

		// Handle the packed texture if applicable
		if (bPackTextures)
		{
			TArray<FColor> MergedTexture;
			MergedTexture.AddZeroed(NumSamples);

			// Merge properties into one texture using the separate colour channels

			// R G B masks
#if PLATFORM_LITTLE_ENDIAN
			const uint32 ColorMask[3] = { 0x0000FF00, 0x00FF0000, 0xFF000000 };
#else // PLATFORM_LITTLE_ENDIAN
			const uint32 ColorMask[3] = { 0x00FF0000, 0x0000FF00, 0x000000FF };
#endif
			for (int32 PropertyIndex = 0; PropertyIndex < 3; ++PropertyIndex)
			{
				EFlattenMaterialProperties Property = (EFlattenMaterialProperties)(PropertyIndex + (int32)EFlattenMaterialProperties::Metallic);
				const bool HasProperty = FlattenMaterial.DoesPropertyContainData(Property) && !FlattenMaterial.IsPropertyConstant(Property);

				if (HasProperty)
				{
					const TArray<FColor>& PropertySamples = FlattenMaterial.GetPropertySamples(Property);
					// OR masked values (samples initialized to zero, so no random data)
					for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
					{
						MergedTexture[SampleIndex].DWColor() |= (PropertySamples[SampleIndex].DWColor() & ColorMask[PropertyIndex]);
					}
				}
			}

			// Create texture using the merged property data
			UTexture2D* PackedTexture = FMaterialUtilities::CreateTexture(InOuter, AssetBasePath + TEXT("T_") + AssetBaseName + TEXT("_MRS"), PackedSize, MergedTexture, TC_Default, TEXTUREGROUP_HierarchicalLOD, RF_Public | RF_Standalone, bSRGB);
			checkf(PackedTexture, TEXT("Failed to create texture"));

			// Setup switches for whether or not properties will be packed into one texture
			FStaticSwitchParameter SwitchParameter;
			SwitchParameter.ParameterName = "PackMetallic";
			SwitchParameter.Value = bPackMetallic;
			SwitchParameter.bOverride = true;
			NewStaticParameterSet.StaticSwitchParameters.Add(SwitchParameter);

			SwitchParameter.ParameterName = "PackSpecular";
			SwitchParameter.Value = bPackSpecular;
			SwitchParameter.bOverride = true;
			NewStaticParameterSet.StaticSwitchParameters.Add(SwitchParameter);

			SwitchParameter.ParameterName = "PackRoughness";
			SwitchParameter.Value = bPackRoughness;
			SwitchParameter.bOverride = true;
			NewStaticParameterSet.StaticSwitchParameters.Add(SwitchParameter);

			// Set up switch and texture values
			OutMaterial->SetTextureParameterValueEditorOnly("PackedTexture", PackedTexture);
		}

		// Emissive is a special case due to the scaling variable
		if (FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive).Num() > 0 && !(FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive).Num() == 1 && FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive)[0] == FColor::Black))
		{
			TEXTURE_MACRO_VECTOR_LINEAR(Emissive, TC_Default, bRGB);

			if (FlattenMaterial.EmissiveScale != 1.0f)
			{
				OutMaterial->SetScalarParameterValueEditorOnly("EmissiveScale", FlattenMaterial.EmissiveScale);
			}
		}

		// Force initializing the static permutations according to the switches we have set
		OutMaterial->UpdateStaticPermutation(NewStaticParameterSet);
		OutMaterial->InitStaticPermutation();

		OutMaterial->PostEditChange();

		return OutMaterial;
	}
};