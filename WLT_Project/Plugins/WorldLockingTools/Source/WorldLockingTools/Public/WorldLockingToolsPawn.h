// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

#include "WorldLockingToolsPawn.generated.h"

UCLASS()
class WORLDLOCKINGTOOLS_API AWorldLockingToolsPawn : public APawn
{
	GENERATED_BODY()

public:
	AWorldLockingToolsPawn(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};