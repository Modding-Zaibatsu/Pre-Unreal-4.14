// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetThumbnail.h"
#include "PropertyEditing.h"
#include "Engine/StaticMesh.h"

enum ECreationModeChoice
{
	CreateNew,
	UseChannel0,
};

enum ELimitModeChoice
{
	Stretching,
	Charts
};

class FLevelOfDetailSettingsLayout;

class FStaticMeshDetails : public IDetailCustomization
{
public:
	FStaticMeshDetails( class FStaticMeshEditor& InStaticMeshEditor );
	~FStaticMeshDetails();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( class IDetailLayoutBuilder& DetailBuilder ) override;

	/** @return true if settings have been changed and need to be applied to the static mesh */
	bool IsApplyNeeded() const;

	/** Applies level of detail changes to the static mesh */
	void ApplyChanges();
private:
	/** Level of detail settings for the details panel */
	TSharedPtr<FLevelOfDetailSettingsLayout> LevelOfDetailSettings;
	/** Static mesh editor */
	class FStaticMeshEditor& StaticMeshEditor;
};


/** 
 * Window handles convex decomposition, settings and controls.
 */
class SConvexDecomposition : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SConvexDecomposition ) :
		_StaticMeshEditorPtr()
		{
		}
		/** The Static Mesh Editor this tool is associated with. */
		SLATE_ARGUMENT( TWeakPtr< IStaticMeshEditor >, StaticMeshEditorPtr )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual ~SConvexDecomposition();

private:
	/** Callback when the "Apply" button is clicked. */
	FReply OnApplyDecomp();

	/** Callback when the "Defaults" button is clicked. */
	FReply OnDefaults();

	/** Assigns the accuracy of hulls based on the spinbox's value. */
	void OnHullAccuracyCommitted(float InNewValue, ETextCommit::Type CommitInfo);

	/** Assigns the accuracy of hulls based on the spinbox's value. */
	void OnHullAccuracyChanged(float InNewValue);

	/** Retrieves the accuracy of hulls created. */
	float GetHullAccuracy() const;

	/** Assigns the max number of hulls based on the spinbox's value. */
	void OnVertsPerHullCountCommitted(int32 InNewValue, ETextCommit::Type CommitInfo);

	/** Assigns the max number of hulls based on the spinbox's value. */
	void OnVertsPerHullCountChanged(int32 InNewValue);

	/** 
	 *	Retrieves the max number of verts per hull allowed.
	 *
	 *	@return			The max number of verts per hull selected.
	 */
	int32 GetVertsPerHullCount() const;

private:
	/** The Static Mesh Editor this tool is associated with. */
	TWeakPtr<IStaticMeshEditor> StaticMeshEditorPtr;

	/** Spinbox for the max number of hulls allowed. */
	TSharedPtr< SSpinBox<float> > HullAccuracy;

	/** The current number of max number of hulls selected. */
	float CurrentHullAccuracy;

	/** Spinbox for the max number of verts per hulls allowed. */
	TSharedPtr< SSpinBox<int32> > MaxVertsPerHull;

	/** The current number of max verts per hull selected. */
	int32 CurrentMaxVertsPerHullCount;

};


class FMeshBuildSettingsLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FMeshBuildSettingsLayout>
{
public:
	FMeshBuildSettingsLayout( TSharedRef<FLevelOfDetailSettingsLayout> InParentLODSettings );
	virtual ~FMeshBuildSettingsLayout();

	const FMeshBuildSettings& GetSettings() const { return BuildSettings; }
	void UpdateSettings(const FMeshBuildSettings& InSettings);

private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) override {}
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) override;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) override;
	virtual void Tick( float DeltaTime ) override{}
	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override { static FName MeshBuildSettings("MeshBuildSettings"); return MeshBuildSettings; }
	virtual bool InitiallyCollapsed() const override { return true; }

	FReply OnApplyChanges();
	ECheckBoxState ShouldRecomputeNormals() const;
	ECheckBoxState ShouldRecomputeTangents() const;
	ECheckBoxState ShouldUseMikkTSpace() const;
	ECheckBoxState ShouldRemoveDegenerates() const;
	ECheckBoxState ShouldBuildAdjacencyBuffer() const;
	ECheckBoxState ShouldBuildReversedIndexBuffer() const;
	ECheckBoxState ShouldUseHighPrecisionTangentBasis() const;
	ECheckBoxState ShouldUseFullPrecisionUVs() const;
	ECheckBoxState ShouldGenerateLightmapUVs() const;
	ECheckBoxState ShouldGenerateDistanceFieldAsIfTwoSided() const;
	int32 GetMinLightmapResolution() const;
	int32 GetSrcLightmapIndex() const;
	int32 GetDstLightmapIndex() const;
	TOptional<float> GetBuildScaleX() const;
	TOptional<float> GetBuildScaleY() const;
	TOptional<float> GetBuildScaleZ() const;
	float GetDistanceFieldResolutionScale() const;
	float GetDistanceFieldBias() const;

	void OnRecomputeNormalsChanged(ECheckBoxState NewState);
	void OnRecomputeTangentsChanged(ECheckBoxState NewState);
	void OnUseMikkTSpaceChanged(ECheckBoxState NewState);
	void OnRemoveDegeneratesChanged(ECheckBoxState NewState);
	void OnBuildAdjacencyBufferChanged(ECheckBoxState NewState);
	void OnBuildReversedIndexBufferChanged(ECheckBoxState NewState);
	void OnUseHighPrecisionTangentBasisChanged(ECheckBoxState NewState);
	void OnUseFullPrecisionUVsChanged(ECheckBoxState NewState);
	void OnGenerateLightmapUVsChanged(ECheckBoxState NewState);
	void OnGenerateDistanceFieldAsIfTwoSidedChanged(ECheckBoxState NewState);
	void OnMinLightmapResolutionChanged( int32 NewValue );
	void OnSrcLightmapIndexChanged( int32 NewValue );
	void OnDstLightmapIndexChanged( int32 NewValue );
	void OnBuildScaleXChanged( float NewScaleX, ETextCommit::Type TextCommitType );
	void OnBuildScaleYChanged( float NewScaleY, ETextCommit::Type TextCommitType );
	void OnBuildScaleZChanged( float NewScaleZ, ETextCommit::Type TextCommitType );

	void OnDistanceFieldResolutionScaleChanged(float NewValue);
	void OnDistanceFieldResolutionScaleCommitted(float NewValue, ETextCommit::Type TextCommitType);
	void OnDistanceFieldBiasChanged(float NewValue);
	void OnDistanceFieldBiasCommitted(float NewValue, ETextCommit::Type TextCommitType);
	FString GetCurrentDistanceFieldReplacementMeshPath() const;
	void OnDistanceFieldReplacementMeshSelected(const FAssetData& AssetData);

private:
	TWeakPtr<FLevelOfDetailSettingsLayout> ParentLODSettings;
	FMeshBuildSettings BuildSettings;
};

class FMeshReductionSettingsLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FMeshReductionSettingsLayout>
{
public:
	FMeshReductionSettingsLayout( TSharedRef<FLevelOfDetailSettingsLayout> InParentLODSettings );
	virtual ~FMeshReductionSettingsLayout();

	const FMeshReductionSettings& GetSettings() const;
	void UpdateSettings(const FMeshReductionSettings& InSettings);
private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) override {}
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) override;
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) override;
	virtual void Tick( float DeltaTime ) override{}
	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override { static FName MeshReductionSettings("MeshReductionSettings"); return MeshReductionSettings; }
	virtual bool InitiallyCollapsed() const override { return true; }

	FReply OnApplyChanges();
	float GetPercentTriangles() const;
	float GetMaxDeviation() const;
	float GetPixelError() const;
	float GetWeldingThreshold() const;
	ECheckBoxState ShouldRecalculateNormals() const;
	float GetHardAngleThreshold() const;

	void OnPercentTrianglesChanged(float NewValue);
	void OnPercentTrianglesCommitted(float NewValue, ETextCommit::Type TextCommitType);
	void OnMaxDeviationChanged(float NewValue);
	void OnMaxDeviationCommitted(float NewValue, ETextCommit::Type TextCommitType);
	void OnPixelErrorChanged(float NewValue);
	void OnPixelErrorCommitted(float NewValue, ETextCommit::Type TextCommitType);
	void OnReductionAmountChanged(float NewValue);
	void OnRecalculateNormalsChanged(ECheckBoxState NewValue);
	void OnWeldingThresholdChanged(float NewValue);
	void OnWeldingThresholdCommitted(float NewValue, ETextCommit::Type TextCommitType);
	void OnHardAngleThresholdChanged(float NewValue);
	void OnHardAngleThresholdCommitted(float NewValue, ETextCommit::Type TextCommitType);

	void OnSilhouetteImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	void OnTextureImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	void OnShadingImportanceChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);

private:
	TWeakPtr<FLevelOfDetailSettingsLayout> ParentLODSettings;
	FMeshReductionSettings ReductionSettings;
	TArray<TSharedPtr<FString> > ImportanceOptions;
	TSharedPtr<STextComboBox> SilhouetteCombo;
	TSharedPtr<STextComboBox> TextureCombo;
	TSharedPtr<STextComboBox> ShadingCombo;
};

class FMeshSectionSettingsLayout : public TSharedFromThis<FMeshSectionSettingsLayout>
{
public:
	FMeshSectionSettingsLayout( IStaticMeshEditor& InStaticMeshEditor, int32 InLODIndex )
		: StaticMeshEditor( InStaticMeshEditor )
		, LODIndex( InLODIndex )
	{}

	virtual ~FMeshSectionSettingsLayout();

	void AddToCategory( IDetailCategoryBuilder& CategoryBuilder );

private:
	
	UStaticMesh& GetStaticMesh() const;

	/**
	* Called by the material list widget when we need to get new materials for the list
	*
	* @param OutMaterials	Handle to a material list builder that materials should be added to
	*/
	void OnGetSectionsForView(class ISectionListBuilder& OutSections, int32 ForLODIndex);

	/**
	* Called when a user drags a new material over a list item to replace it
	*
	* @param NewMaterial	The material that should replace the existing material
	* @param PrevMaterial	The material that should be replaced
	* @param SlotIndex		The index of the slot on the component where materials should be replaces
	* @param bReplaceAll	If true all materials in the slot should be replaced not just ones using PrevMaterial
	*/
	void OnSectionChanged(int32 ForLODIndex, int32 SectionIndex, int32 NewMaterialSlotIndex, FName NewMaterialSlotName);
	
	/**
	* Called by the material list widget on generating each name widget
	*
	* @param Material		The material that is being displayed
	* @param SlotIndex		The index of the material slot
	*/
	TSharedRef<SWidget> OnGenerateCustomNameWidgetsForSection(int32 ForLODIndex, int32 SectionIndex);

	/**
	* Called by the material list widget on generating each thumbnail widget
	*
	* @param Material		The material that is being displayed
	* @param SlotIndex		The index of the material slot
	*/
	TSharedRef<SWidget> OnGenerateCustomSectionWidgetsForSection(int32 ForLODIndex, int32 SectionIndex);

	ECheckBoxState DoesSectionCastShadow(int32 SectionIndex) const;
	void OnSectionCastShadowChanged(ECheckBoxState NewState, int32 SectionIndex);
	ECheckBoxState DoesSectionCollide(int32 SectionIndex) const;
	bool SectionCollisionEnabled() const;
	FText GetCollisionEnabledToolTip() const;
	void OnSectionCollisionChanged(ECheckBoxState NewState, int32 SectionIndex);

	ECheckBoxState IsSectionHighlighted(int32 SectionIndex) const;
	void OnSectionHighlightedChanged(ECheckBoxState NewState, int32 SectionIndex);
	ECheckBoxState IsSectionIsolatedEnabled(int32 SectionIndex) const;
	void OnSectionIsolatedChanged(ECheckBoxState NewState, int32 SectionIndex);

	void CallPostEditChange(UProperty* PropertyChanged=nullptr);
	
	IStaticMeshEditor& StaticMeshEditor;
	int32 LODIndex;
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


class FMeshMaterialsLayout : public TSharedFromThis<FMeshMaterialsLayout>
{
public:
	FMeshMaterialsLayout(IStaticMeshEditor& InStaticMeshEditor)
		: StaticMeshEditor(InStaticMeshEditor)
	{}

	virtual ~FMeshMaterialsLayout();

	void AddToCategory(IDetailCategoryBuilder& CategoryBuilder);

private:
	UStaticMesh& GetStaticMesh() const;
	FReply AddMaterialSlot();
	FText GetMaterialArrayText() const;

	void GetMaterials(class IMaterialListBuilder& ListBuilder);
	void OnMaterialChanged(UMaterialInterface* NewMaterial, UMaterialInterface* PrevMaterial, int32 SlotIndex, bool bReplaceAll);
	TSharedRef<SWidget> OnGenerateWidgetsForMaterial(UMaterialInterface* Material, int32 SlotIndex);
	void OnResetMaterialToDefaultClicked(UMaterialInterface* Material, int32 SlotIndex);

	FText GetOriginalImportMaterialNameText(int32 MaterialIndex) const;
	FText GetMaterialNameText(int32 MaterialIndex) const;
	void OnMaterialNameCommitted(const FText& InValue, ETextCommit::Type CommitType, int32 MaterialIndex);
	void OnMaterialNameChanged(const FText& InValue, int32 MaterialIndex);
	bool CanDeleteMaterialSlot(int32 MaterialIndex) const;
	void OnDeleteMaterialSlot(int32 MaterialIndex);
	TSharedRef<SWidget> OnGetMaterialSlotUsedByMenuContent(int32 MaterialIndex);
	FText GetFirstMaterialSlotUsedBySection(int32 MaterialIndex) const;

	/* If the material list is dirty this function will return true */
	bool OnMaterialListDirty();

	ECheckBoxState IsShadowCastingEnabled(int32 SlotIndex) const;
	void OnShadowCastingChanged(ECheckBoxState NewState, int32 SlotIndex);

	void CallPostEditChange(UProperty* PropertyChanged = nullptr);

	IStaticMeshEditor& StaticMeshEditor;
	
	/* This is to know if material are used by any LODs sections. */
	TMap<int32, TArray<FSectionLocalizer>> MaterialUsedMap;
};

/** 
 * Window for adding and removing LOD.
 */
class FLevelOfDetailSettingsLayout : public TSharedFromThis<FLevelOfDetailSettingsLayout>
{
public:
	FLevelOfDetailSettingsLayout( FStaticMeshEditor& StaticMeshEditor );
	virtual ~FLevelOfDetailSettingsLayout();
	
	void AddToDetailsPanel( IDetailLayoutBuilder& DetailBuilder );

	/** Returns true if settings have been changed and an Apply is needed to update the asset. */
	bool IsApplyNeeded() const;

	/** Apply current LOD settings to the mesh. */
	void ApplyChanges();

private:

	/** Creates the UI for Current LOD panel */
	void AddLODLevelCategories( class IDetailLayoutBuilder& DetailBuilder );

	/** Callbacks. */
	FReply OnApply();
	void OnLODCountChanged(int32 NewValue);
	void OnLODCountCommitted(int32 InValue, ETextCommit::Type CommitInfo);
	int32 GetLODCount() const;

	void OnMinLODChanged(int32 NewValue);
	void OnMinLODCommitted(int32 InValue, ETextCommit::Type CommitInfo);
	int32 GetMinLOD() const;

	float GetLODScreenSize(int32 LODIndex)const;
	FText GetLODScreenSizeTitle(int32 LODIndex) const;
	bool CanChangeLODScreenSize() const;
	void OnLODScreenSizeChanged(float NewValue, int32 LODIndex);
	void OnLODScreenSizeCommitted(float NewValue, ETextCommit::Type CommitType, int32 LODIndex);

	void OnBuildSettingsExpanded(bool bIsExpanded, int32 LODIndex);
	void OnReductionSettingsExpanded(bool bIsExpanded, int32 LODIndex);
	void OnSectionSettingsExpanded(bool bIsExpanded, int32 LODIndex);
	void OnLODGroupChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	bool IsAutoLODEnabled() const;
	ECheckBoxState IsAutoLODChecked() const;
	void OnAutoLODChanged(ECheckBoxState NewState);
	void OnImportLOD(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	void UpdateLODNames();
	FText GetLODCountTooltip() const;
	FText GetMinLODTooltip() const;

private:

	/** The Static Mesh Editor this tool is associated with. */
	FStaticMeshEditor& StaticMeshEditor;

	/** Pool for material thumbnails. */
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	/** LOD group options. */
	TArray<FName> LODGroupNames;
	TArray<TSharedPtr<FString> > LODGroupOptions;

	/** LOD import options */
	TArray<TSharedPtr<FString> > LODNames;

	/** Simplification options for each LOD level (in the LOD Chain tool). */
	TSharedPtr<FMeshReductionSettingsLayout> ReductionSettingsWidgets[MAX_STATIC_MESH_LODS];
	TSharedPtr<FMeshBuildSettingsLayout> BuildSettingsWidgets[MAX_STATIC_MESH_LODS];
	TSharedPtr<FMeshSectionSettingsLayout> SectionSettingsWidgets[MAX_STATIC_MESH_LODS];

	TSharedPtr<FMeshMaterialsLayout> MaterialsLayoutWidget;

	/** ComboBox widget for the LOD Group property */
	TSharedPtr<STextComboBox> LODGroupComboBox;

	/** The display factors at which LODs swap */
	float LODScreenSizes[MAX_STATIC_MESH_LODS];

	/** Helper value that corresponds to the 'Number of LODs' spinbox.*/
	int32 LODCount;

	/** Whether certain parts of the UI are expanded so changes persist across
	    recreating the UI. */
	bool bBuildSettingsExpanded[MAX_STATIC_MESH_LODS];
	bool bReductionSettingsExpanded[MAX_STATIC_MESH_LODS];
	bool bSectionSettingsExpanded[MAX_STATIC_MESH_LODS];
};