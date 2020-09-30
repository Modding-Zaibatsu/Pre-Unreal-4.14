// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "KismetWidgetsPrivatePCH.h"
#include "SSingleObjectDetailsPanel.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"

/////////////////////////////////////////////////////
// SSingleObjectDetailsPanel

void SSingleObjectDetailsPanel::Construct(const FArguments& InArgs, bool bAutomaticallyObserveViaGetObjectToObserve, bool bAllowSearch)
{
	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs(/*bUpdateFromSelection=*/ false, /*bLockable=*/ false, bAllowSearch, FDetailsViewArgs::HideNameArea, /*bHideSelectionTip=*/ true);
	DetailsViewArgs.HostCommandList = InArgs._HostCommandList;

	PropertyView = EditModule.CreateDetailView(DetailsViewArgs);
	
	bAutoObserveObject = bAutomaticallyObserveViaGetObjectToObserve;

	// Create the border that all of the content will get stuffed into
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding( 3.0f, 2.0f )
		[
			PopulateSlot(PropertyView.ToSharedRef())
		]
	];
}

UObject* SSingleObjectDetailsPanel::GetObjectToObserve() const
{
	return NULL;
}

void SSingleObjectDetailsPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bAutoObserveObject)
	{
		UObject* CurrentObject = GetObjectToObserve();
		if (LastObservedObject.Get() != CurrentObject)
		{
			LastObservedObject = CurrentObject;

			TArray<UObject*> SelectedObjects;
			if (CurrentObject != NULL)
			{
				SelectedObjects.Add(CurrentObject);
			}

			SetPropertyWindowContents(SelectedObjects);
		}
	}
}

void SSingleObjectDetailsPanel::SetPropertyWindowContents(TArray<UObject*> Objects)
{
	if (FSlateApplication::IsInitialized())
	{
		check(PropertyView.IsValid());
		PropertyView->SetObjects(Objects);
	}
}

TSharedRef<SWidget> SSingleObjectDetailsPanel::PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget)
{
	return PropertyEditorWidget;
}
