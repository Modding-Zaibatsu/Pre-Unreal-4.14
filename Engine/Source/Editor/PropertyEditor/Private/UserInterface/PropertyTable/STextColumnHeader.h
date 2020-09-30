// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SColumnHeader.h"
#include "TextPropertyTableCellPresenter.h"
#include "IPropertyTableUtilities.h"
#include "IPropertyTableCell.h"
#include "PropertyTableCell.h"
#include "SPropertyTableCell.h"

class STextColumnHeader : public SColumnHeader
{
public:

	SLATE_BEGIN_ARGS( STextColumnHeader )
		: _Style( TEXT("PropertyTable") )
	{}
		SLATE_ARGUMENT( FName, Style )
		SLATE_ARGUMENT( TSharedPtr< IPropertyTableCustomColumn >, Customization )
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InViewModel			The UI logic not specific to slate
	 */
	void Construct( const FArguments& InArgs, const TSharedRef< class IPropertyTableColumn >& InPropertyTableColumn, const TSharedRef< class IPropertyTableUtilities >& InPropertyUtilities )
	{
		Style = InArgs._Style;

		SColumnHeader::FArguments ColumnArgs;
		ColumnArgs.Style( Style );
		ColumnArgs.Customization( InArgs._Customization );

		SColumnHeader::Construct( ColumnArgs, InPropertyTableColumn, InPropertyUtilities );
	}

	virtual TSharedRef< SWidget > GenerateCell( const TSharedRef< class IPropertyTableRow >& PropertyTableRow ) override
	{
		TSharedRef< IPropertyTableCell > Cell = Column->GetCell( PropertyTableRow );

		TSharedPtr< IPropertyTableCellPresenter > CellPresenter( NULL );

		if ( Customization.IsValid() )
		{
			CellPresenter = Customization->CreateCellPresenter( Cell, Utilities.ToSharedRef(), Style );
		}

		if ( !CellPresenter.IsValid() && Cell->IsBound() )
		{
			TSharedRef< FPropertyEditor > PropertyEditor = FPropertyEditor::Create( Cell->GetNode().ToSharedRef(), Utilities.ToSharedRef() );
			CellPresenter = MakeShareable( new FTextPropertyTableCellPresenter( PropertyEditor, Utilities.ToSharedRef() ) );
		}

		return SNew( SPropertyTableCell, Cell )
			.Presenter( CellPresenter )
			.Style( Style );
	}


private:

	FName Style;
};