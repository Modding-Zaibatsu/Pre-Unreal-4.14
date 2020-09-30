// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FMatineeActorDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();
private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

	FReply OnOpenMatineeForActor();
private:
	/** The selected matinee actor */
	TWeakObjectPtr<AMatineeActor> MatineeActor;
};
