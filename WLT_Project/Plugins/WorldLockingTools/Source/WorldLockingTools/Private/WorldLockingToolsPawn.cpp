// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "WorldLockingToolsPawn.h"

AWorldLockingToolsPawn::AWorldLockingToolsPawn(const FObjectInitializer& ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// Create a scene component hierarchy that works with World Locking Tools.
	// WLT requires the player camera to have a parent and grandparent component.
	// The grandparent component can not be the default scene root.
	USceneComponent* PawnRoot = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, "DefaultSceneRoot");
	PawnRoot->SetMobility(EComponentMobility::Movable);

	USceneComponent* AdjustmentFrame = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(PawnRoot, "AdjustmentFrame");
	AdjustmentFrame->SetMobility(EComponentMobility::Movable);
	AdjustmentFrame->AttachToComponent(PawnRoot, FAttachmentTransformRules::KeepRelativeTransform);

	USceneComponent* CameraRoot = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(AdjustmentFrame, "CameraRoot");
	CameraRoot->SetMobility(EComponentMobility::Movable);
	CameraRoot->AttachToComponent(AdjustmentFrame, FAttachmentTransformRules::KeepRelativeTransform);

	UCameraComponent* PlayerCamera = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(CameraRoot, "PlayerCamera");
	PlayerCamera->SetMobility(EComponentMobility::Movable);
	PlayerCamera->AttachToComponent(CameraRoot, FAttachmentTransformRules::KeepRelativeTransform);
}

void AWorldLockingToolsPawn::BeginPlay()
{
	Super::BeginPlay();
}

void AWorldLockingToolsPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AWorldLockingToolsPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

