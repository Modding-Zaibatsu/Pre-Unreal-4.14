// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
 
#pragma once
#include "SListView.h"


/**
 * A TileView widget is a list which arranges its items horizontally until there is no more space then creates a new row.
 * Items are spaced evenly horizontally.
 */
template <typename ItemType>
class STileView : public SListView<ItemType>
{
public:
	typedef typename TListTypeTraits< ItemType >::NullableType NullableItemType;

	typedef typename TSlateDelegates< ItemType >::FOnGenerateRow FOnGenerateRow;
	typedef typename TSlateDelegates< ItemType >::FOnItemScrolledIntoView FOnItemScrolledIntoView;
	typedef typename TSlateDelegates< ItemType >::FOnMouseButtonClick FOnMouseButtonClick;
	typedef typename TSlateDelegates< ItemType >::FOnMouseButtonDoubleClick FOnMouseButtonDoubleClick;
	typedef typename TSlateDelegates< NullableItemType >::FOnSelectionChanged FOnSelectionChanged;

	using FOnWidgetToBeRemoved = typename SListView<ItemType>::FOnWidgetToBeRemoved;

public:

	SLATE_BEGIN_ARGS( STileView<ItemType> )
		: _OnGenerateTile()
		, _OnTileReleased()
		, _ListItemsSource( static_cast<TArray<ItemType>*>(nullptr) ) //@todo Slate Syntax: Initializing from nullptr without a cast
		, _ItemHeight(128)
		, _ItemWidth(128)
		, _ItemAlignment(EListItemAlignment::EvenlyDistributed)
		, _OnContextMenuOpening()
		, _OnMouseButtonClick()
		, _OnMouseButtonDoubleClick()
		, _OnSelectionChanged()
		, _SelectionMode(ESelectionMode::Multi)
		, _ClearSelectionOnClick(true)
		, _ExternalScrollbar()
		, _ScrollbarVisibility(EVisibility::Visible)
		, _AllowOverscroll(EAllowOverscroll::Yes)
		, _ConsumeMouseWheel( EConsumeMouseWheel::WhenScrollingPossible )
		, _WheelScrollMultiplier( WheelScrollAmount )
		, _HandleGamepadEvents( true )
		{}

		SLATE_EVENT( FOnGenerateRow, OnGenerateTile )

		SLATE_EVENT( FOnWidgetToBeRemoved, OnTileReleased )

		SLATE_EVENT( FOnTableViewScrolled, OnTileViewScrolled )

		SLATE_EVENT( FOnItemScrolledIntoView, OnItemScrolledIntoView )

		SLATE_ARGUMENT( const TArray<ItemType>* , ListItemsSource )

		SLATE_ATTRIBUTE( float, ItemHeight )

		SLATE_ATTRIBUTE( float, ItemWidth )

		SLATE_ATTRIBUTE( EListItemAlignment, ItemAlignment )

		SLATE_EVENT( FOnContextMenuOpening, OnContextMenuOpening )

		SLATE_EVENT( FOnMouseButtonClick, OnMouseButtonClick )

		SLATE_EVENT( FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick )

		SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )

		SLATE_ATTRIBUTE( ESelectionMode::Type, SelectionMode )

		SLATE_ARGUMENT ( bool, ClearSelectionOnClick )

		SLATE_ARGUMENT( TSharedPtr<SScrollBar>, ExternalScrollbar )

		SLATE_ATTRIBUTE(EVisibility, ScrollbarVisibility)

		SLATE_ARGUMENT( EAllowOverscroll, AllowOverscroll );

		SLATE_ARGUMENT( EConsumeMouseWheel, ConsumeMouseWheel );

		SLATE_ARGUMENT( float, WheelScrollMultiplier );

		SLATE_ARGUMENT( bool, HandleGamepadEvents );

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const typename STileView<ItemType>::FArguments& InArgs )
	{
		this->OnGenerateRow = InArgs._OnGenerateTile;
		this->OnRowReleased = InArgs._OnTileReleased;
		this->OnItemScrolledIntoView = InArgs._OnItemScrolledIntoView;
		
		this->ItemsSource = InArgs._ListItemsSource;
		this->OnContextMenuOpening = InArgs._OnContextMenuOpening;
		this->OnClick = InArgs._OnMouseButtonClick;
		this->OnDoubleClick = InArgs._OnMouseButtonDoubleClick;
		this->OnSelectionChanged = InArgs._OnSelectionChanged;
		this->SelectionMode = InArgs._SelectionMode;

		this->bClearSelectionOnClick = InArgs._ClearSelectionOnClick;

		this->AllowOverscroll = InArgs._AllowOverscroll;
		this->ConsumeMouseWheel = InArgs._ConsumeMouseWheel;

		this->WheelScrollMultiplier = InArgs._WheelScrollMultiplier;

		this->bHandleGamepadEvents = InArgs._HandleGamepadEvents;

		// Check for any parameters that the coder forgot to specify.
		FString ErrorString;
		{
			if ( !this->OnGenerateRow.IsBound() )
			{
				ErrorString += TEXT("Please specify an OnGenerateTile. \n");
			}

			if ( this->ItemsSource == nullptr )
			{
				ErrorString += TEXT("Please specify a ListItemsSource. \n");
			}
		}

		if (ErrorString.Len() > 0)
		{
			// Let the coder know what they forgot
			this->ChildSlot
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(ErrorString))
			];
		}
		else
		{
			// Make the TableView
			this->ConstructChildren(InArgs._ItemWidth, InArgs._ItemHeight, InArgs._ItemAlignment, TSharedPtr<SHeaderRow>(), InArgs._ExternalScrollbar, InArgs._OnTileViewScrolled);
			if (this->ScrollBar.IsValid())
			{
				this->ScrollBar->SetUserVisibility(InArgs._ScrollbarVisibility);
			}
		}
	}

	STileView( ETableViewMode::Type InListMode = ETableViewMode::Tile )
	: SListView<ItemType>( InListMode )
	{
	}

public:

	// SWidget overrides

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		const TArray<ItemType>& ItemsSourceRef = (*this->ItemsSource);

		// Don't respond to key-presses containing "Alt" as a modifier
		if ( ItemsSourceRef.Num() > 0 && !InKeyEvent.IsAltDown() )
		{
			// Check for selection manipulation keys that differ from SListView (Left, Right)
			if ( InKeyEvent.GetKey() == EKeys::Left )
			{
				int32 SelectionIndex = ( !TListTypeTraits<ItemType>::IsPtrValid(this->SelectorItem) ) ? 0 : ItemsSourceRef.Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( this->SelectorItem ) );

				if ( SelectionIndex > 0 )
				{
					// Select the previous item
					this->KeyboardSelect(ItemsSourceRef[SelectionIndex - 1], InKeyEvent);
				}

				return FReply::Handled();
			}
			else if ( InKeyEvent.GetKey() == EKeys::Right )
			{
				int32 SelectionIndex = ( !TListTypeTraits<ItemType>::IsPtrValid(this->SelectorItem) ) ? 0 : ItemsSourceRef.Find( TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType( this->SelectorItem ) );

				if ( SelectionIndex < ItemsSourceRef.Num() - 1 )
				{
					// Select the next item
					this->KeyboardSelect(ItemsSourceRef[SelectionIndex + 1], InKeyEvent);
				}

				return FReply::Handled();
			}
		}

		return SListView<ItemType>::OnKeyDown(MyGeometry, InKeyEvent);
	}

public:	

	virtual STableViewBase::FReGenerateResults ReGenerateItems( const FGeometry& MyGeometry ) override
	{
		// Clear all the items from our panel. We will re-add them in the correct order momentarily.
		this->ClearWidgets();
		
		const TArray<ItemType>* SourceItems = this->ItemsSource;
		if ( SourceItems != nullptr )
		{
			// Item width and height is constant by design.
			const float AllottedWidth = MyGeometry.Size.X;
			const float ItemWidth = this->GetItemWidth();
			const float ItemHeight = this->GetItemHeight();

			const int32 NumItemsWide = GetNumItemsWide();
			const int32 NumItemsPaddedToFillLastRow = (SourceItems->Num() % NumItemsWide != 0)
				? SourceItems->Num() + NumItemsWide - SourceItems->Num() % NumItemsWide
				: SourceItems->Num();
			const double RowsPerScreen = MyGeometry.Size.Y / ItemHeight;
			const double EndOfListOffset = NumItemsPaddedToFillLastRow - NumItemsWide * RowsPerScreen;
			const double ClampedScrollOffset = FMath::Clamp(STableViewBase::ScrollOffset, 0.0, EndOfListOffset);
			const bool bAtEndOfList = (STableViewBase::ScrollOffset >= ClampedScrollOffset);
			const float LayoutScaleMultiplier = MyGeometry.GetAccumulatedLayoutTransform().GetScale();
			
			// Once we run out of vertical and horizontal space, we stop generating widgets.
			float WidthUsedSoFar = 0.0f;
			float HeightUsedSoFar = 0.0f;
			float WidgetHeightSoFar = 0.0f;
			// Index of the item at which we start generating based on how far scrolled down we are
			int32 StartIndex = FMath::Max( 0, FMath::FloorToInt(ClampedScrollOffset / NumItemsWide) * NumItemsWide );

			// Let the WidgetGenerator know that we are starting a pass so that it can keep track of data items and widgets.
			this->WidgetGenerator.OnBeginGenerationPass();

			// Actually generate the widgets.
			bool bKeepGenerating = true;
			bool bNewRow = true;
			bool bFirstRow = true;
			double NumRowsShownOnScreen = 0;
			for( int32 ItemIndex = StartIndex; bKeepGenerating && ItemIndex < SourceItems->Num(); ++ItemIndex )
			{
				const ItemType& CurItem = (*SourceItems)[ItemIndex];

				if (bNewRow)
				{
					bNewRow = false;
					WidgetHeightSoFar += ItemHeight;

					float RowFraction = 1.0f;
					if (bFirstRow)
					{
						bFirstRow = false;
						RowFraction = 1.0f - FMath::Max(FMath::Fractional(ClampedScrollOffset / NumItemsWide), 0.0f);
					}

					HeightUsedSoFar += ItemHeight * RowFraction;

					if (HeightUsedSoFar > MyGeometry.Size.Y)
					{
						NumRowsShownOnScreen += FMath::Max(1.0f - ((HeightUsedSoFar - MyGeometry.Size.Y) / ItemHeight), 0.0f);
					}
					else
					{
						NumRowsShownOnScreen += RowFraction;
					}
				}

				const float GeneratedItemHeight = SListView<ItemType>::GenerateWidgetForItem(CurItem, ItemIndex, StartIndex, LayoutScaleMultiplier);

				// The widget used up some of the available horizontal space.
				WidthUsedSoFar += ItemWidth;

				// Check to see if we have completed a row of widgets
				if ( WidthUsedSoFar + ItemWidth > AllottedWidth )
				{
					// A new row of widgets was added.
					WidthUsedSoFar = 0;
					bNewRow = true;

					// Hack: Added "+ ItemHeight". This unfixes a previous bug which caused TileViews to 
					// work when placed in a SScrollBox.
					// !!!!!! THIS IS A PORTAL ONLY CHANGE !!!!!! 
					// If found outside of portal repositories please
					// Contact: Barnabas.McManners or Justin.Sargent.

					// Stop when we've generated a widget that's partially clipped by the bottom of the list.
					if ( HeightUsedSoFar >= MyGeometry.Size.Y /*<HACK>*/+ ItemHeight /*</HACK>*/)
					{
						bKeepGenerating = false;
					}
				}
			}

			// We have completed the generation pass. The WidgetGenerator will clean up unused Widgets.
			this->WidgetGenerator.OnEndGenerationPass();

			return STableViewBase::FReGenerateResults(ClampedScrollOffset, WidgetHeightSoFar, NumRowsShownOnScreen, bAtEndOfList);
		}

		return STableViewBase::FReGenerateResults(0, 0, 0, false);

	}

	virtual int32 GetNumItemsBeingObserved() const override
	{
		const int32 NumItemsBeingObserved = this->ItemsSource == nullptr ? 0 : this->ItemsSource->Num();
		const int32 NumItemsWide = GetNumItemsWide();
		
		int32 NumEmptySpacesAtEnd = 0;
		if ( NumItemsWide > 0 )
		{
			NumEmptySpacesAtEnd = NumItemsWide - (NumItemsBeingObserved % NumItemsWide);
			if ( NumEmptySpacesAtEnd >= NumItemsWide )
			{
				NumEmptySpacesAtEnd = 0;
			}
		}

		return NumItemsBeingObserved + NumEmptySpacesAtEnd;
	}

protected:

	virtual float ScrollBy( const FGeometry& MyGeometry, float ScrollByAmountInSlateUnits, EAllowOverscroll InAllowOverscroll ) override
	{
		// Working around a CLANG bug, where all the base class
		// members require an explicit namespace resolution.
		typedef STableViewBase S;

		const bool bWholeListVisible = S::ScrollOffset == 0 && S::bWasAtEndOfList;
		if ( InAllowOverscroll == EAllowOverscroll::Yes && S::Overscroll.ShouldApplyOverscroll( S::ScrollOffset == 0, S::bWasAtEndOfList, ScrollByAmountInSlateUnits ) )
		{
			const float UnclampedScrollDelta = ScrollByAmountInSlateUnits / GetNumItemsWide();
			const float ActuallyScrolledBy = S::Overscroll.ScrollBy( UnclampedScrollDelta );
			if (ActuallyScrolledBy != 0.0f)
			{
				this->RequestListRefresh();
			}
			return ActuallyScrolledBy;
		}
		else if (!bWholeListVisible)
		{
			const float ItemHeight = this->GetItemHeight();
			const double NewScrollOffset = this->ScrollOffset + ( ( ScrollByAmountInSlateUnits * GetNumItemsWide() ) / ItemHeight );

			return this->ScrollTo( NewScrollOffset );
		}

		return 0;
	}

	virtual int32 GetNumItemsWide() const override
	{
		const float ItemWidth = this->GetItemWidth();
		const int32 NumItemsWide = ItemWidth > 0 ? FMath::FloorToInt(this->PanelGeometryLastTick.Size.X / ItemWidth) : 1;
		return FMath::Max(1, NumItemsWide);
	}

	/**
	* If there is a pending request to scroll an item into view, do so.
	*
	* @param ListViewGeometry  The geometry of the listView; can be useful for centering the item.
	*/
	virtual typename SListView<ItemType>::EScrollIntoViewResult ScrollIntoView(const FGeometry& ListViewGeometry) override
	{
		if (TListTypeTraits<ItemType>::IsPtrValid(this->ItemToScrollIntoView) && this->ItemsSource != nullptr)
		{
			const int32 IndexOfItem = this->ItemsSource->Find(TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType(this->ItemToScrollIntoView));
			if (IndexOfItem != INDEX_NONE)
			{
				const float NumItemsHigh = ListViewGeometry.Size.Y / this->GetItemHeight();
				float NumLiveWidgets = this->GetNumLiveWidgets();
				if (NumLiveWidgets == 0 && this->IsPendingRefresh())
				{
					// Use the last number of widgets on screen to estimate if we actually need to scroll.
					NumLiveWidgets = this->LastGenerateResults.ExactNumRowsOnScreen;

					// If we still don't have any widgets, we're not in a situation where we can scroll an item into view
					// (probably as nothing has been generated yet), so we'll defer this again until the next frame
					if (NumLiveWidgets == 0)
					{
						return SListView<ItemType>::EScrollIntoViewResult::Deferred;
					}
				}

				// Only scroll the item into view if it's not already in the visible range
				const int32 NumItemsWide = GetNumItemsWide();
				const float Index = (IndexOfItem / NumItemsWide) * NumItemsWide;
				const float IndexPlusOne = ((IndexOfItem / NumItemsWide) + 1) * NumItemsWide;
				if (Index < this->ScrollOffset || IndexPlusOne > (this->ScrollOffset + NumItemsHigh * NumItemsWide))
				{
					// Scroll the top of the listview to the item in question
					float NewScrollOffset = Index;
					// Center the list view on the item in question.
					NewScrollOffset -= ((NumItemsHigh - 1.0f) * NumItemsWide * 0.5f);
					// And clamp the scroll offset within the allowed limits
					NewScrollOffset = FMath::Clamp(NewScrollOffset, 0.0f, GetNumItemsBeingObserved() - NumItemsWide * NumItemsHigh);

					this->SetScrollOffset(NewScrollOffset);
				}

				this->RequestListRefresh();

				this->ItemToNotifyWhenInView = this->ItemToScrollIntoView;
			}

			TListTypeTraits<ItemType>::ResetPtr(this->ItemToScrollIntoView);
		}

		return SListView<ItemType>::EScrollIntoViewResult::Success;
	}
};
