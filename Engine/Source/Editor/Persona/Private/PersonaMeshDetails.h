// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomNodeBuilder.h"
/**
 * Struct to uniquely identify clothing applied to a material section
 * Contains index into the ClothingAssets array and the submesh index.
 */
struct FClothAssetSubmeshIndex
{
	FClothAssetSubmeshIndex(int32 InAssetIndex, int32 InSubmeshIndex)
		:	AssetIndex(InAssetIndex)
		,	SubmeshIndex(InSubmeshIndex)
	{}

	int32 AssetIndex;
	int32 SubmeshIndex;

	bool operator==(const FClothAssetSubmeshIndex& Other) const
	{
		return (AssetIndex	== Other.AssetIndex 
			&& SubmeshIndex	== Other.SubmeshIndex
			);
	}
};

struct FClothingComboInfo
{
	/* Per-material clothing combo boxes, array size must be same to # of sections */
	TArray<TSharedPtr< STextComboBox >>		ClothingComboBoxes;
	/* Clothing combo box strings */
	TArray<TSharedPtr<FString> >			ClothingComboStrings;
	/* Mapping from a combo box string to the asset and submesh it was generated from */
	TMap<FString, FClothAssetSubmeshIndex>	ClothingComboStringReverseLookup;
	/* The currently-selected index from each clothing combo box */
	TArray<int32>							ClothingComboSelectedIndices;
};

struct FSectionLocalizer
{
	FSectionLocalizer(int32 InLODIndex, int32 InSectionIndex)
		: LODIndex(InLODIndex)
		, SectionIndex(InSectionIndex)
	{}

	bool operator==(const FSectionLocalizer& Other) const
	{
		return (LODIndex == Other.LODIndex && SectionIndex == Other.SectionIndex);
	}

	bool operator!=(const FSectionLocalizer& Other) const
	{
		return !((*this) == Other);
	}

	int32 LODIndex;
	int32 SectionIndex;
};

class FSkelMeshReductionSettingsLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FSkelMeshReductionSettingsLayout>
{
public:
	FSkelMeshReductionSettingsLayout(int32 InLODIndex, TSharedRef<class FPersonaMeshDetails> InParentLODSettings, TSharedPtr<IPropertyHandle> InBoneToRemoveProperty);
	virtual ~FSkelMeshReductionSettingsLayout();

	const FSkeletalMeshOptimizationSettings& GetSettings() const;
	void UpdateSettings(const FSkeletalMeshOptimizationSettings& InSettings);
private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override {}
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override{}
	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override { static FName MeshReductionSettings("MeshReductionSettings"); return MeshReductionSettings; }
	virtual bool InitiallyCollapsed() const override { return true; }

	FReply OnApplyChanges();
	float GetPercentTriangles() const;
	float GetMaxDeviation() const;
	float GetWeldingThreshold() const;
	ECheckBoxState ShouldRecomputeTangents() const;
	float GetHardAngleThreshold() const;
	int32 GetMaxBonesPerVertex() const;
	int32 GetBaseLOD() const;

	void OnPercentTrianglesChanged(float NewValue);
	void OnMaxDeviationChanged(float NewValue);
	void OnReductionAmountChanged(float NewValue);
	void OnRecomputeTangentsChanged(ECheckBoxState NewValue);
	void OnWeldingThresholdChanged(float NewValue);
	void OnHardAngleThresholdChanged(float NewValue);
	void OnMaxBonesPerVertexChanged(int32 NewValue);
	void OnBaseLODChanged(int32 NewBasedLOD);

	void OnSilhouetteImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	void OnTextureImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	void OnShadingImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	void OnSkinningImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);

	void UpdateBonesToRemoveProperties(int32 LODIndex);
	void RefreshBonesToRemove();

private:
	int32 LODIndex;
	TWeakPtr<class FPersonaMeshDetails> ParentLODSettings;
	TSharedPtr<IPropertyHandle>	BoneToRemoveProperty;
	FSkeletalMeshOptimizationSettings ReductionSettings;

	TArray<TSharedPtr<FString> > ImportanceOptions;
	TArray<TSharedPtr<FString> > SimplificationOptions;
	TSharedPtr<STextComboBox> SilhouetteCombo;
	TSharedPtr<STextComboBox> TextureCombo;
	TSharedPtr<STextComboBox> ShadingCombo;
	TSharedPtr<STextComboBox> SkinningCombo;
};

class FPersonaMeshDetails : public IDetailCustomization
{
public:
	FPersonaMeshDetails(TSharedRef<class IPersonaToolkit> InPersonaToolkit) : PersonaToolkitPtr(InPersonaToolkit) {}

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance(TSharedRef<class IPersonaToolkit> InPersonaToolkit);

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

private:

	FReply AddMaterialSlot();

	FText GetMaterialArrayText() const;

	/**
	 * Called by the material list widget when we need to get new materials for the list
	 *
	 * @param OutMaterials	Handle to a material list builder that materials should be added to
	 */
	void OnGetSectionsForView( class ISectionListBuilder& OutSections, int32 LODIndex );

	/**
	 * Called when a user drags a new material over a list item to replace it
	 *
	 * @param NewMaterial	The material that should replace the existing material
	 * @param PrevMaterial	The material that should be replaced
	 * @param SlotIndex		The index of the slot on the component where materials should be replaces
	 * @param bReplaceAll	If true all materials in the slot should be replaced not just ones using PrevMaterial
	 */
	void OnSectionChanged(int32 LODIndex, int32 SectionIndex, int32 NewMaterialSlotIndex, FName NewMaterialSlotName);

	/**
	* Called by the material list widget when we need to get new materials for the list
	*
	* @param OutMaterials	Handle to a material list builder that materials should be added to
	*/
	void OnGetMaterialsForArray(class IMaterialListBuilder& OutMaterials, int32 LODIndex);

	/**
	* Called when a user drags a new material over a list item to replace it
	*
	* @param NewMaterial	The material that should replace the existing material
	* @param PrevMaterial	The material that should be replaced
	* @param SlotIndex		The index of the slot on the component where materials should be replaces
	* @param bReplaceAll	If true all materials in the slot should be replaced not just ones using PrevMaterial
	*/
	void OnMaterialArrayChanged(UMaterialInterface* NewMaterial, UMaterialInterface* PrevMaterial, int32 SlotIndex, bool bReplaceAll, int32 LODIndex);

	
	/**
	 * Called by the material list widget on generating each name widget
	 *
	 * @param Material		The material that is being displayed
	 * @param SlotIndex		The index of the material slot
	 */
	TSharedRef<SWidget> OnGenerateCustomNameWidgetsForSection(int32 LodIndex, int32 SectionIndex);

	/**
	 * Called by the material list widget on generating each thumbnail widget
	 *
	 * @param Material		The material that is being displayed
	 * @param SlotIndex		The index of the material slot
	 */
	TSharedRef<SWidget> OnGenerateCustomSectionWidgetsForSection(int32 LODIndex, int32 SectionIndex);

	FText GetMaterialNameText(int32 MaterialIndex)const ;
	void OnMaterialNameCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 MaterialIndex);
	void OnMaterialNameChanged(const FText& InValue, int32 MaterialIndex);

	FText GetOriginalImportMaterialNameText(int32 MaterialIndex)const;

	/**
	* Called by the material list widget on generating each thumbnail widget
	*
	* @param Material		The material that is being displayed
	* @param MaterialIndex	The index of the material slot
	*/
	TSharedRef<SWidget> OnGenerateCustomMaterialWidgetsForMaterialArray(UMaterialInterface* Material, int32 MaterialIndex, int32 LODIndex);

	/* If the material list is dirty this function will return true */
	bool OnMaterialListDirty();

	bool CanDeleteMaterialSlot(int32 MaterialIndex) const;

	void OnDeleteMaterialSlot(int32 MaterialIndex);

	TSharedRef<SWidget> OnGetMaterialSlotUsedByMenuContent(int32 MaterialIndex);

	FText GetFirstMaterialSlotUsedBySection(int32 MaterialIndex) const;

	/**
	 * Handler for check box display based on whether the material is highlighted
	 *
	 * @param SectionIndex	The material section that is being tested
	 */
	ECheckBoxState IsSectionSelected(int32 SectionIndex) const;

	/**
	 * Handler for changing highlight status on a material
	 *
	 * @param SectionIndex	The material section that is being tested
	 */
	void OnSectionSelectedChanged(ECheckBoxState NewState, int32 SectionIndex);

	/**
	* Handler for check box display based on whether the material is isolated
	*
	* @param SectionIndex	The material section that is being tested
	*/
	ECheckBoxState IsIsolateSectionEnabled(int32 SectionIndex) const;

	/**
	* Handler for changing isolated status on a material
	*
	* @param SectionIndex	The material section that is being tested
	*/
	void OnSectionIsolatedChanged(ECheckBoxState NewState, int32 SectionIndex);

	/**
	 * Handler for check box display based on whether the material has shadow casting enabled
	 *
	 * @param MaterialIndex	The material index which a section in a specific LOD model has
	 */
	ECheckBoxState IsShadowCastingEnabled(int32 MaterialIndex) const;

	/**
	 * Handler for changing shadow casting status on a material
	 *
	 * @param MaterialIndex	The material index which a section in a specific LOD model has
	 */
	void OnShadowCastingChanged(ECheckBoxState NewState, int32 MaterialIndex);

	/**
	* Handler for check box display based on whether this section does recalculate normal or not
	*
	* @param MaterialIndex	The material index which a section in a specific LOD model has
	*/
	ECheckBoxState IsRecomputeTangentEnabled(int32 MaterialIndex) const;

	/**
	* Handler for changing recalulate normal status on a material
	*
	* @param MaterialIndex	The material index which a section in a specific LOD model has
	*/
	void OnRecomputeTangentChanged(ECheckBoxState NewState, int32 MaterialIndex);

	/**
	* Handler for check box display based on whether the material has shadow casting enabled
	*
	* @param LODIndex	The LODIndex we want to change
	* @param SectionIndex	The SectionIndex we change the RecomputeTangent
	*/
	ECheckBoxState IsSectionShadowCastingEnabled(int32 LODIndex, int32 SectionIndex) const;

	/**
	* Handler for changing shadow casting status on a section
	*
	* @param LODIndex	The LODIndex we want to change
	* @param SectionIndex	The SectionIndex we change the RecomputeTangent
	*/
	void OnSectionShadowCastingChanged(ECheckBoxState NewState, int32 LODIndex, int32 SectionIndex);

	/**
	* Handler for check box display based on whether this section does recalculate normal or not
	*
	* @param LODIndex	The LODIndex we want to change
	* @param SectionIndex	The SectionIndex we change the RecomputeTangent
	*/
	ECheckBoxState IsSectionRecomputeTangentEnabled(int32 LODIndex, int32 SectionIndex) const;

	/**
	* Handler for changing recalulate normal status on a section
	*
	* @param LODIndex	The LODIndex we want to change
	* @param SectionIndex	The SectionIndex we change the RecomputeTangent
	*/
	void OnSectionRecomputeTangentChanged(ECheckBoxState NewState, int32 LODIndex, int32 SectionIndex);

	/**
	 * Handler for enabling delete button on materials
	 *
	 * @param SectionIndex - index of the section to check
	 */
	bool CanDeleteMaterialElement(int32 LODIndex, int32 SectionIndex) const;

	/**
	 * Handler for deleting material elements
	 * 
	 * @Param SectionIndex - material section to remove
	 */
	FReply OnDeleteButtonClicked(int32 LODIndex, int32 SectionIndex);

	/** Creates the UI for Current LOD panel */
	void AddLODLevelCategories(IDetailLayoutBuilder& DetailLayout);

	bool IsDuplicatedMaterialIndex(int32 LODIndex, int32 MaterialIndex);

	/** Get a material index from LOD index and section index */
	int32 GetMaterialIndex(int32 LODIndex, int32 SectionIndex) const;

	/** for LOD settings category */
	void CustomizeLODSettingsCategories(IDetailLayoutBuilder& DetailLayout);

	void OnImportLOD(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, IDetailLayoutBuilder* DetailLayout);
	void UpdateLODNames();
	int32 GetLODCount() const;
	void OnLODCountChanged(int32 NewValue);
	void OnLODCountCommitted(int32 InValue, ETextCommit::Type CommitInfo);
	FText GetLODCountTooltip() const;
	FText GetLODImportedText(int32 LODIndex) const;

	FText GetMaterialSlotNameText(int32 MaterialIndex) const;

	/** apply LOD changes if the user modified LOD reduction settings */
	FReply OnApplyChanges();
	/** Removes the specified lod from the skeletal mesh */
	FReply RemoveOneLOD(int32 LODIndex);
	/** Remove Bones again */
	FReply RemoveBones(int32 LODIndex);
	/** hide properties which don't need to be showed to end users */
	void HideUnnecessaryProperties(IDetailLayoutBuilder& DetailLayout);

	// Handling functions for post process blueprint selection combo box
	void OnPostProcessBlueprintChanged(IDetailLayoutBuilder* DetailBuilder);
	FString GetCurrentPostProcessBlueprintPath() const;
	bool OnShouldFilterPostProcessBlueprint(const FAssetData& AssetData) const;
	void OnSetPostProcessBlueprint(const FAssetData& AssetData, TSharedRef<IPropertyHandle> BlueprintProperty);

	/** Access the persona toolkit ptr. It should always be valid in the lifetime of this customization */
	TSharedRef<IPersonaToolkit> GetPersonaToolkit() const { check(PersonaToolkitPtr.IsValid()); return PersonaToolkitPtr.Pin().ToSharedRef(); }

public:

	bool IsApplyNeeded() const;
	bool IsGenerateAvailable() const;
	void ApplyChanges(int32 DesiredLOD, const FSkeletalMeshOptimizationSettings& ReductionSettings);
	void ApplyChanges();
	FText GetApplyButtonText() const;

private:
	// Container for the objects to display
	TWeakObjectPtr<USkeletalMesh> SkeletalMeshPtr;

	// Reference the persona toolkit
	TWeakPtr<class IPersonaToolkit> PersonaToolkitPtr;

	IDetailLayoutBuilder* MeshDetailLayout;

	/** LOD import options */
	TArray<TSharedPtr<FString> > LODNames;
	/** Helper value that corresponds to the 'Number of LODs' spinbox.*/
	int32 LODCount;

	/** Simplification options for each LOD level */
	TArray<TSharedPtr<FSkelMeshReductionSettingsLayout>> ReductionSettingsWidgets;

	/* This is to know if material are used by any LODs sections. */
	TMap<int32, TArray<FSectionLocalizer>> MaterialUsedMap;

#if WITH_APEX_CLOTHING
private:

	// info about clothing combo boxes for multiple LOD
	TArray<FClothingComboInfo>				ClothingComboLODInfos;


	/* Clothing combo box functions */
	EVisibility IsClothingComboBoxVisible(int32 LODIndex, int32 SectionIndex) const;
	FString HandleSectionsComboBoxGetRowText(TSharedPtr<FString> Section, int32 LODIndex, int32 SectionIndex);
	void HandleSectionsComboBoxSelectionChanged(TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo, int32 LODIndex, int32 SectionIndex );
	void UpdateComboBoxStrings();

	/* Generate slate UI for Clothing category */
	void CustomizeClothingProperties(class IDetailLayoutBuilder& DetailLayout, class IDetailCategoryBuilder& ClothingFilesCategory);

	/* Generate each ClothingAsset array entry */
	void OnGenerateElementForClothingAsset( TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout );

	/* Make uniform grid widget for Apex details */
	TSharedRef<SUniformGridPanel> MakeApexDetailsWidget(int32 AssetIndex) const;

	/* Opens dialog to add a new clothing asset */
	FReply OnOpenClothingFileClicked(IDetailLayoutBuilder* DetailLayout);

	/* Reimports a clothing asset */ 
	FReply OnReimportApexFileClicked(int32 AssetIndex, IDetailLayoutBuilder* DetailLayout);

	/* Removes a clothing asset */ 
	FReply OnRemoveApexFileClicked(int32 AssetIndex, IDetailLayoutBuilder* DetailLayout);


	/* if physics properties are changed, then save to the clothing asset */
	void UpdateClothPhysicsProperties(int32 AssetIndex);
#endif // #if WITH_APEX_CLOTHING

};
