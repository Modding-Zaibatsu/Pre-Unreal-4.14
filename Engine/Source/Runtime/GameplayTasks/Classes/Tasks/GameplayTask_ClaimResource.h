// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GameplayTask.h"
#include "GameplayTask_ClaimResource.generated.h"

class UGameplayTaskResource;

UCLASS(BlueprintType)
class GAMEPLAYTASKS_API UGameplayTask_ClaimResource : public UGameplayTask
{
	GENERATED_BODY()
public:
	UGameplayTask_ClaimResource(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "Priority, TaskInstanceName"))
	static UGameplayTask_ClaimResource* ClaimResource(TScriptInterface<IGameplayTaskOwnerInterface> InTaskOwner, TSubclassOf<UGameplayTaskResource> ResourceClass, const uint8 Priority = 192, const FName TaskInstanceName = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "Priority, TaskInstanceName"))
	static UGameplayTask_ClaimResource* ClaimResources(TScriptInterface<IGameplayTaskOwnerInterface> InTaskOwner, TArray<TSubclassOf<UGameplayTaskResource> > ResourceClasses, const uint8 Priority = 192, const FName TaskInstanceName = NAME_None);

	static UGameplayTask_ClaimResource* ClaimResource(IGameplayTaskOwnerInterface& InTaskOwner, const TSubclassOf<UGameplayTaskResource> ResourceClass, const uint8 Priority = FGameplayTasks::DefaultPriority, const FName TaskInstanceName = NAME_None);
	static UGameplayTask_ClaimResource* ClaimResources(IGameplayTaskOwnerInterface& InTaskOwner, const TArray<TSubclassOf<UGameplayTaskResource> >& ResourceClasses, const uint8 Priority = FGameplayTasks::DefaultPriority, const FName TaskInstanceName = NAME_None);
};
