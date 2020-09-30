// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/////////////////////////////////////////////////////
// FSkeletalControlNodeDetails 

class FSkeletalControlNodeDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
	// End of IDetailCustomization interface

protected:
	void OnGenerateElementForPropertyPin(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder, FName CategoryName);

	/** Returns the current value of the checkbox being displayed for the bShowPin property */
	ECheckBoxState GetShowPinValueForProperty(TSharedRef<IPropertyHandle> InElementProperty) const;

	/** Helper function for changing the value of the bShowPin checkbox to update the property */
	void OnShowPinChanged(ECheckBoxState InNewState, TSharedRef<IPropertyHandle> InElementProperty);

private:
	IDetailCategoryBuilder* DetailCategory;
};