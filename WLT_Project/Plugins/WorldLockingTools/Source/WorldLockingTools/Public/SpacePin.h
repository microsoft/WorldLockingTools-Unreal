// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"

#include "FrozenWorldPlugin.h"

#include "FrozenWorldEngine.h"
#include "AttachmentPoint.h"

#include "Components/SceneComponent.h"
#include "SpacePin.generated.h"

/// Component helper for pinning the world locked space at a single reference point.
/// 
/// This component captures the initial pose of its actor, and then a second pose. It then
/// adds that pair to the WorldLocking Alignment Manager. The manager then negotiates between all
/// such added pins, based on the current head pose, to generate a frame-to-frame mapping aligning
/// the Frozen Space, i.e. Unreal's World Space, such that the pins match up as well as possible.
/// Another way to phrase this is:
///	Given an arbitrary pose (the "modeling pose"),
///	and a pose aligned somehow to the real world (the "world locked pose"),
///	apply a correction to the camera such that a virtual object with coordinates of the modeling pose
///	will appear overlaid on the real world at the position and orientation described by the locked pose.
/// For this component, the locked pose must come in via one of the following three APIs:
///	 SetFrozenPose(FTransform) with input pose in Frozen Space, which includes pinning.
///	 SetSpongyPose(FTransform) with input pose in Spongy Space, which is the space of the camera's parent,
///		 and is the same space the camera moves in, and that native APIs return values in (e.g. XR).
///	 SetLockedPose(FTransform) with input pose in Locked Space, which is the space stabilized by
///		 the Frozen World engine DLL but excluding pinning.
/// Note that since the Frozen Space is shifted by the AlignmentManager, calling SetFrozenPose(p) with the same Pose p
/// twice is probably an error, since the Pose p would refer to different a location after the first call.
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = WorldLockingTools, Category = "World Locking Tools")
class USpacePin : public USceneComponent
{
	GENERATED_BODY()

public:
	USpacePin(const FObjectInitializer& ObjectInitializer);

	void ResetModelingPose();

public:
	/// Transform pose to Locked Space and pass through.
	/// @param frozenPose Pose in frozen space.
	/// @param IgnoreYaw Ignore yaw from frozenPose.
	/// @param IgnorePitch Ignore pitch from frozenPose.
	/// @param IgnoreRoll Ignore roll from frozenPose.
	/// @param FlipTransformAroundY Flip the input transform.  Used to adjust QR transforms which are flipped from typical Unreal space.
	/// @param PositionTolerance Amount of positional change to ignore in subsequent calls.  Used to prevent jitter with noisy input transforms.
	/// @param RotationTolerance Amount of rotational change to ignore in subsequent calls.  Used to prevent jitter with noisy input transforms.
	UFUNCTION(BlueprintCallable, Category = "World Locking Tools")
	void SetFrozenPose(FTransform frozenPose, bool FlipTransformAroundY = false, bool IgnoreYaw = false, bool IgnorePitch = true, bool IgnoreRoll = true, float PositionTolerance = 1, float RotationTolerance = 3);

	/// Transform pose to Locked Space and pass through.
	/// @param spongyPose Pose in spongy space.
	/// @param IgnoreYaw Ignore yaw from spongyPose.
	/// @param IgnorePitch Ignore pitch from spongyPose.
	/// @param IgnoreRoll Ignore roll from spongyPose.
	/// @param FlipTransformAroundY Flip the input transform.  Used to adjust QR transforms which are flipped from typical Unreal space.
	/// @param PositionTolerance Amount of positional change to ignore in subsequent calls.  Used to prevent jitter with noisy input transforms.
	/// @param RotationTolerance Amount of rotational change to ignore in subsequent calls.  Used to prevent jitter with noisy input transforms.
	UFUNCTION(BlueprintCallable, Category = "World Locking Tools")
	void SetSpongyPose(FTransform spongyPose, bool FlipTransformAroundY = false, bool IgnoreYaw = false, bool IgnorePitch = true, bool IgnoreRoll = true, float PositionTolerance = 1, float RotationTolerance = 3);

	/// Record the locked pose and push data to the alignment manager.
	/// @param lockedPose Pose to send to the alignment manager
	/// @param PositionTolerance Amount of positional change to ignore in subsequent calls.  Used to prevent jitter with noisy input transforms.
	/// @param RotationTolerance Amount of rotational change to ignore in subsequent calls.  Used to prevent jitter with noisy input transforms.
	UFUNCTION(BlueprintCallable, Category = "World Locking Tools")
	void SetLockedPose(FTransform lockedPose, float PositionTolerance = 1, float RotationTolerance = 3);

private:
	WorldLockingTools::FAttachmentPoint::FAdjustLocationDelegate LocationHandler;

	FTransform LockedPose = FTransform::Identity;
	FTransform restorePoseLocal = FTransform::Identity;
	FTransform modelingPoseParent = FTransform::Identity;

	TSharedPtr<WorldLockingTools::FAttachmentPoint> AttachmentPoint = nullptr;

	FTransform GlobalFromParent();
	FTransform ParentFromGlobal();
	FTransform ModelingPoseGlobal();

	FTransform ExtractModelPose();
	void RemoveScale(FTransform& pose, FVector scale);
	FTransform AddScale(FTransform pose, FVector scale);

	void RestoreOnLoad();
	void Reset();

public:
	FrozenWorld_AnchorId AnchorID = FrozenWorld_AnchorId_UNKNOWN;
	FString AnchorName;

	/// Whether this space pin is in active use pinning space
	bool PinActive()
	{
		return AnchorID != FrozenWorld_AnchorId_UNKNOWN 
			&& AnchorID != FrozenWorld_AnchorId_INVALID;
	}

private:
	void PushAlignmentData();
	void SendAlignmentData();

	void CheckAttachment();
	void ForceAttachment();
	void ReleaseAttachment();

	void OnLocationUpdate(FTransform adjustment);
};
