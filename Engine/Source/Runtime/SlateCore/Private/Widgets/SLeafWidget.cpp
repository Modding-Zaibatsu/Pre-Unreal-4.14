// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"


/* Static initialization
 *****************************************************************************/

FNoChildren SLeafWidget::NoChildrenInstance;


/* SLeafWidget interface
 *****************************************************************************/

void SLeafWidget::SetVisibility( TAttribute<EVisibility> InVisibility )
{
	SWidget::SetVisibility( InVisibility );
}

FChildren* SLeafWidget::GetChildren( )
{
	return &SLeafWidget::NoChildrenInstance;
}


void SLeafWidget::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	// Nothing to arrange; Leaf Widgets do not have children.
}
