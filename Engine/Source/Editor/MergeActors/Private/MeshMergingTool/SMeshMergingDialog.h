// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/MeshMerging.h"
#include "PropertyEditorModule.h"

class FMeshMergingTool;
class IDetailsView;
class UMeshMergingSettingsObject;
class IPropertyHandle;
class FDetailWidgetRow;
class IDetailChildrenBuilder;

/** Data structure used to keep track of the selected mesh components, and whether or not they should be incorporated in the merge */
struct FMeshComponentData
{
	FMeshComponentData(UStaticMeshComponent* InMeshComponent)
		: MeshComponent(InMeshComponent )
		, bShouldIncorporate( true )
	{}

	/** Mesh component extracted from selected actors */
	TWeakObjectPtr<UStaticMeshComponent> MeshComponent;
	/** Flag determining whether or not this component should be incorporated into the merge */
	bool bShouldIncorporate;
};

/*-----------------------------------------------------------------------------
   SMeshMergingDialog
-----------------------------------------------------------------------------*/
class SMeshMergingDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMeshMergingDialog)
	{
	}

	SLATE_END_ARGS()

public:
	/** **/
	SMeshMergingDialog();
	~SMeshMergingDialog();

	/** SWidget functions */
	void Construct(const FArguments& InArgs, FMeshMergingTool* InTool);	
	
	/** Getter functionality */
	const TArray<TSharedPtr<FMeshComponentData>>& GetSelectedMeshComponents() const { return SelectedMeshComponents; }
	const int32 GetNumSelectedMeshComponents() const { return NumSelectedMeshComponents; }

	/** Resets the state of the UI and flags it for refreshing */
	void Reset();
private:	
	/** Begin override SCompoundWidget */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	/** End override SCompoundWidget */

	/** Creates and sets up the settings view element*/
	void CreateSettingsView();	

	/** Delegate for the creation of the list view item's widget */
	TSharedRef<ITableRow> MakeComponentListItemWidget(TSharedPtr<FMeshComponentData> StaticMeshComponent, const TSharedRef<STableViewBase>& OwnerTable);

	/** Delegate to determine whether or not the UI elements should be enabled (determined by number of selected actors / mesh components) */
	bool GetContentEnabledState() const;
		
	/** Delegates for replace actors checkbox UI element*/
	ECheckBoxState GetReplaceSourceActors() const;
	void SetReplaceSourceActors(ECheckBoxState NewValue);
	
	/** Editor delgates for map and selection changes */
	void OnLevelSelectionChanged(UObject* Obj);
	void OnMapChange(uint32 MapFlags);
	void OnNewCurrentLevel();

	/** Updates SelectedMeshComponent array according to retrieved mesh components from editor selection*/
	void UpdateSelectedStaticMeshComponents();	
	/** Stores the individual check box states for the currently selected mesh components */
	void StoreCheckBoxState();
private:
	/** Owning mesh merging tool */
	FMeshMergingTool* Tool;
	/** List of mesh components extracted from editor selection */
	TArray<TSharedPtr<FMeshComponentData>> SelectedMeshComponents;	
	/** List view ui element */
	TSharedPtr<SListView<TSharedPtr<FMeshComponentData>>> MeshComponentsListView;
	/** Map of keeping track of checkbox states for each selected static mesh component (used to restore state when listview is refreshed) */
	TMap<UStaticMeshComponent*, ECheckBoxState> StoredCheckBoxStates;
	/** Settings view ui element ptr */
	TSharedPtr<IDetailsView> SettingsView;
	/** Cached pointer to mesh merging setting singleton object */
	UMeshMergingSettingsObject* MergeSettings;
	/** List view state tracking data */
	bool bRefreshListView;
	int NumSelectedMeshComponents;
};
