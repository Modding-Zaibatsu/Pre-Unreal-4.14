// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Particles/ParticleModule.h"
#include "ParticleModuleCameraBase.generated.h"

UCLASS(editinlinenew, hidecategories=Object, abstract, meta=(DisplayName = "Camera"))
class UParticleModuleCameraBase : public UParticleModule
{
	GENERATED_UCLASS_BODY()

	virtual bool CanTickInAnyThread() override
	{
		return false;
	}
};

