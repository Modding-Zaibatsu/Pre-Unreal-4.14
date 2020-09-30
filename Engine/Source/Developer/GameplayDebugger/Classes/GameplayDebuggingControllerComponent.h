// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//////////////////////////////////////////////////////////////////////////
// THIS CLASS IS NOW DEPRECATED AND WILL BE REMOVED IN NEXT VERSION
// Please check GameplayDebugger.h for details.

#pragma once
#include "TimerManager.h"
#include "InputCore.h"
#include "GameplayDebuggingTypes.h"
#include "GameplayDebugger.h"
#include "GameFramework/HUD.h"
#include "Engine/DebugCameraController.h"
#include "GameplayDebuggingControllerComponent.generated.h"

class AGameplayDebuggingReplicator;

UCLASS(config = Engine)
class GAMEPLAYDEBUGGER_API UGameplayDebuggingControllerComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

#if WITH_HOT_RELOAD_CTORS
	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	UGameplayDebuggingControllerComponent(FVTableHelper& Helper);
#endif // WITH_HOT_RELOAD_CTORS

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void BeginDestroy() override;
	virtual void OnRegister() override;
	
	void SetPlayerOwner(APlayerController* PC) { PlayerOwner = PC; }

	void BindActivationKeys();
	void OnActivationKeyPressed();
	void OnActivationKeyReleased();

	void SetActiveViews(uint32 InActiveViews);
	virtual uint32 GetActiveViews();

	bool IsViewActive(EAIDebugDrawDataView::Type View) const;
	void EnableActiveView(EAIDebugDrawDataView::Type View, bool bEnable);
	void ToggleActiveView(EAIDebugDrawDataView::Type View)
	{
		EnableActiveView(View, IsViewActive(View) ? false : true);
	}

	const FInputChord& GetActivationKey() const { return ActivationKey; }

	/** periodic update of navmesh data */
	void UpdateNavMeshTimer();

	float GetUpdateNavMeshTimeRemaining() const;

	TWeakObjectPtr<ADebugCameraController> GetDebugCameraController() { return DebugCameraController; }

protected:

	UPROPERTY(Transient)
	class AGameplayDebuggingHUDComponent*	OnDebugAIHUD;

	UPROPERTY(Transient)
	class AActor* DebugAITargetActor;

	UPROPERTY(Transient)
	class UInputComponent* AIDebugViewInputComponent;

	UPROPERTY(Transient)
	class UInputComponent* DebugCameraInputComponent;

	void OpenDebugTool();
	void CloseDebugTool();

	void EnableTargetSelection(bool bEnable);

	bool CanToggleView();
	virtual void ToggleAIDebugView_SetView0();
	virtual void ToggleAIDebugView_SetView1();
	virtual void ToggleAIDebugView_SetView2();
	virtual void ToggleAIDebugView_SetView3();
	virtual void ToggleAIDebugView_SetView4();
	virtual void ToggleAIDebugView_SetView5();
	virtual void ToggleAIDebugView_SetView6();
	virtual void ToggleAIDebugView_SetView7();
	virtual void ToggleAIDebugView_SetView8();
	virtual void ToggleAIDebugView_SetView9();
	virtual void CycleDetailsView();

	virtual void BindAIDebugViewKeys(class UInputComponent*& InputComponent);
	AGameplayDebuggingReplicator* GetDebuggingReplicator() const;
	virtual void ToggleDebugCamera();
	virtual void ToggleOnScreenDebugMessages();
	virtual void ToggleGameHUD();

	TWeakObjectPtr<APlayerController> PlayerOwner;
	TWeakObjectPtr<ADebugCameraController> DebugCameraController;

	/** Handle for efficient management of UpdateNavMesh timer */
	FTimerHandle TimerHandle_UpdateNavMeshTimer;

	const float KeyPressActivationTime;
	float ControlKeyPressedTime;
	float PlayersComponentRequestTime;
	uint32 bToolActivated : 1;
	uint32 bWaitingForOwnersComponent : 1;

public:
	UPROPERTY(config)
	FInputChord ActivationKey;

	UPROPERTY(config)
	FInputChord CategoryZeroBind;

	UPROPERTY(config)
	FInputChord CategoryOneBind;

	UPROPERTY(config)
	FInputChord CategoryTwoBind;

	UPROPERTY(config)
	FInputChord CategoryThreeBind;

	UPROPERTY(config)
	FInputChord CategoryFourBind;

	UPROPERTY(config)
	FInputChord CategoryFiveBind;

	UPROPERTY(config)
	FInputChord CategorySixBind;

	UPROPERTY(config)
	FInputChord CategorySevenBind;

	UPROPERTY(config)
	FInputChord CategoryEightBind;

	UPROPERTY(config)
	FInputChord CategoryNineBind;

	UPROPERTY(config)
	FInputChord CycleDetailsViewBind;

	UPROPERTY(config)
	FInputChord DebugCameraBind;

	UPROPERTY(config)
	FInputChord OnScreenDebugMessagesBind;

	UPROPERTY(config)
	FInputChord GameHUDBind;
};

UCLASS(NotBlueprintable, Transient, NotBlueprintType, hidedropdown, hidecategories = Actor, notplaceable)
class AGaneplayDebuggerProxyHUD : public AHUD
{
	GENERATED_UCLASS_BODY()

	FFontRenderInfo TextRenderInfo;

	//~ Begin AActor Interface
	virtual void PostRender() override;
	//~ End AActor Interface

	TWeakObjectPtr<AHUD> RedirectedHUD;
};

