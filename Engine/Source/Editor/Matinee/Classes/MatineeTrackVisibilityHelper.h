// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "MatineeTrackVisibilityHelper.generated.h"

UCLASS()
class UMatineeTrackVisibilityHelper : public UInterpTrackHelper
{
	GENERATED_UCLASS_BODY()

	void OnAddKeyTextEntry(const FString& ChosenText, IMatineeBase* Matinee, UInterpTrack* Track);

public:

	// UInterpTrackHelper Interface

	virtual	bool PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const override;
	virtual void  PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const override;
};

