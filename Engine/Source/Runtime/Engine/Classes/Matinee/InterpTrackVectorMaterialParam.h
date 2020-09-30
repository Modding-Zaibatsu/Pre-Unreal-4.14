// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "InterpTrackVectorMaterialParam.generated.h"

UCLASS(MinimalAPI, meta=( DisplayName = "Vector Material Parameter Track" ))
class UInterpTrackVectorMaterialParam : public UInterpTrackVectorBase
{
	GENERATED_UCLASS_BODY()

	/** Materials whose parameters we want to change and the references to those materials. */
	UPROPERTY(EditAnywhere, Category=InterpTrackVectorMaterialParam)
	TArray<class UMaterialInterface*> TargetMaterials;

	/** Name of parameter in the MaterialInstance which this track will modify over time. */
	UPROPERTY(EditAnywhere, Category=InterpTrackVectorMaterialParam)
	FName ParamName;


	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//End UObject Interface

	//~ Begin UInterpTrack Interface.
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	//~ End UInterpTrack Interface.
};



