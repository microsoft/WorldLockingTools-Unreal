// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "SpacePin.h"

#include "AlignmentManager.h"
#include "FrozenWorldPoseExtensions.h"

#include "Async/Async.h"
#include "Kismet/KismetSystemLibrary.h"

USpacePin::USpacePin(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	AnchorName = UKismetSystemLibrary::GetObjectName((const UObject*)GetOwner());

	ResetModelingPose();

	WorldLockingTools::FAlignmentManager::OnAlignmentManagerLoad.AddUObject(this, &USpacePin::RestoreOnLoad);
	WorldLockingTools::FAlignmentManager::OnAlignmentManagerReset.AddUObject(this, &USpacePin::Reset);
}

void USpacePin::SetFrozenPose(FTransform frozenPose, bool FlipTransformAroundY, bool IgnoreYaw, bool IgnorePitch, bool IgnoreRoll,
	float PositionTolerance, float RotationTolerance)
{
	if (FlipTransformAroundY)
	{
		frozenPose.SetRotation(frozenPose.GetRotation() * FQuat(FVector::RightVector, PI));
	}

	if (IgnoreYaw || IgnorePitch || IgnoreRoll)
	{
		FRotator rot = frozenPose.GetRotation().Rotator();

		if (IgnoreYaw)
		{
			rot.Yaw = 0;
		}
		if (IgnorePitch)
		{
			rot.Pitch = 0;
		}
		if (IgnoreRoll)
		{
			rot.Roll = 0;
		}

		frozenPose.SetRotation(FQuat(rot));
	}

	FTransform LockedFromFrozen = WorldLockingTools::FFrozenWorldPlugin::Get()->LockedFromFrozen();
	SetLockedPose(WorldLockingTools::FFrozenWorldPoseExtensions::Multiply(LockedFromFrozen, frozenPose), 
		PositionTolerance, RotationTolerance);
}

void USpacePin::SetSpongyPose(FTransform spongyPose, bool FlipTransformAroundY, bool IgnoreYaw, bool IgnorePitch, bool IgnoreRoll, 
	float PositionTolerance, float RotationTolerance)
{
	if (FlipTransformAroundY)
	{
		spongyPose.SetRotation(spongyPose.GetRotation() * FQuat(FVector::RightVector, PI));
	}

	if (IgnoreYaw || IgnorePitch || IgnoreRoll)
	{
		FRotator rot = spongyPose.GetRotation().Rotator();

		if (IgnoreYaw)
		{
			rot.Yaw = 0;
		}
		if (IgnorePitch)
		{
			rot.Pitch = 0;
		}
		if (IgnoreRoll)
		{
			rot.Roll = 0;
		}

		spongyPose.SetRotation(FQuat(rot));
	}

	SetLockedPose(WorldLockingTools::FFrozenWorldPoseExtensions::Multiply(WorldLockingTools::FFrozenWorldPlugin::Get()->LockedFromSpongy(), spongyPose), 
		PositionTolerance, RotationTolerance);
}

void USpacePin::SetLockedPose(FTransform lockedPose, float PositionTolerance, float RotationTolerance)
{
	FTransform diff = LockedPose.GetRelativeTransform(lockedPose);
	if (diff.GetLocation().IsNearlyZero(PositionTolerance) &&
		FRotator(diff.GetRotation()).IsNearlyZero(RotationTolerance))
	{
		return;
	}

	LockedPose = lockedPose;

	PushAlignmentData();
	SendAlignmentData();
}

/// <summary>
/// Communicate the data from this point to the alignment manager.
/// </summary>
void USpacePin::PushAlignmentData()
{
	if (PinActive())
	{
		WorldLockingTools::FAlignmentManager::Get()->RemoveAlignmentAnchor(AnchorID);
	}
	AnchorID = WorldLockingTools::FAlignmentManager::Get()->AddAlignmentAnchor(AnchorName, ModelingPoseGlobal(), LockedPose);
}

/// <summary>
/// Notify the manager that all necessary updates have been submitted and
/// are ready for processing.
/// </summary>
void USpacePin::SendAlignmentData()
{
	WorldLockingTools::FAlignmentManager::Get()->SendAlignmentAnchors();

	CheckAttachment();

	SetRelativeTransform(restorePoseLocal);
}

/// <summary>
/// Check if an attachment point is needed, if so then setup and make current.
/// </summary>
void USpacePin::CheckAttachment()
{
	if (!PinActive())
	{
		return;
	}
	ForceAttachment();
}

/// <summary>
/// Ensure that there is an attachment, and it is positioned up to date.
/// </summary>
void USpacePin::ForceAttachment()
{
	if (AttachmentPoint == nullptr)
	{
		LocationHandler.BindUObject(this, &USpacePin::OnLocationUpdate);

		AttachmentPoint = WorldLockingTools::FFragmentManager::Get()->CreateAttachmentPoint(LockedPose.GetLocation(), nullptr, LocationHandler, nullptr);
	}
	else
	{
		WorldLockingTools::FFragmentManager::Get()->TeleportAttachmentPoint(AttachmentPoint, LockedPose.GetLocation(), nullptr);
	}
}

/// <summary>
/// Dispose of any previously created attachment point.
/// </summary>
void USpacePin::ReleaseAttachment()
{
	if (AttachmentPoint != nullptr)
	{
		WorldLockingTools::FFragmentManager::Get()->ReleaseAttachmentPoint(AttachmentPoint);
		AttachmentPoint = nullptr;
	}
}

/// <summary>
/// Callback for refit operations. Apply adjustment transform to locked pose.
/// </summary>
/// <param name="adjustment">Adjustment to apply.</param>
void USpacePin::OnLocationUpdate(FTransform adjustment)
{
	LockedPose = WorldLockingTools::FFrozenWorldPoseExtensions::Multiply(adjustment, LockedPose);
}

/// <summary>
/// Reset the modeling pose to the current transform.
/// 
/// In normal usage, the modeling pose is the transform as set in Unreal and as cached at start.
/// In some circumstances, such as creation of pins from script, it may be convenient to set the 
/// transform after start. In this case, the change of transform should be recorded by a
/// call to ResetModelingPose().
/// This must happen before the modeling pose is used implicitly by a call to set the 
/// virtual pose, via SetFrozenPose, SetSpongyPose, or SetLockedPose.
/// </summary>
void USpacePin::ResetModelingPose()
{
	restorePoseLocal = GetRelativeTransform();

	modelingPoseParent = WorldLockingTools::FFrozenWorldPoseExtensions::Multiply(ParentFromGlobal(), ExtractModelPose());
	// Undo any scale. This will be multiplied back in in ModelingPoseGlobal().
	RemoveScale(modelingPoseParent, GetComponentTransform().GetScale3D());
}

/// <summary>
/// First of the pair of poses submitted to alignment manager for alignment.
/// </summary>
FTransform USpacePin::ModelingPoseGlobal()
{
	FTransform rescaledModelingPose = AddScale(modelingPoseParent, GetComponentTransform().GetScale3D());
	return WorldLockingTools::FFrozenWorldPoseExtensions::Multiply(GlobalFromParent(), rescaledModelingPose);
}

/// <summary>
/// Return the Pose transforming from parent space to global space.
/// 
/// If the SpacePin has no parent, this will be the identity Pose.
/// </summary>
FTransform USpacePin::GlobalFromParent()
{
	FTransform globalFromParent = FTransform::Identity;
	if (GetAttachParent() != nullptr)
	{
		globalFromParent = GetAttachParent()->GetSocketTransform(GetAttachSocketName());
	}

	return globalFromParent;
}

/// <summary>
/// Return the Pose transforming from global space to the parent's space.
/// </summary>
FTransform USpacePin::ParentFromGlobal()
{
	return WorldLockingTools::FFrozenWorldPoseExtensions::Inverse(GlobalFromParent());
}

FTransform USpacePin::ExtractModelPose()
{
	return GetComponentTransform();
}

void USpacePin::RemoveScale(FTransform& pose, FVector scale)
{
	FVector p = pose.GetLocation();
	p *= FVector(1.0f / scale.X, 1.0f / scale.Y, 1.0f / scale.Z);
	
	pose.SetLocation(p);
}

FTransform USpacePin::AddScale(FTransform pose, FVector scale)
{
	FTransform scaledPose = pose;

	FVector p = scaledPose.GetLocation();
	p *= scale;

	scaledPose.SetLocation(p);

	return scaledPose;
}

/// <summary>
/// Callback on notification of the alignment manager's database to check
/// if this preset has been persisted, and restore it to operation if it has.
/// </summary>
void USpacePin::RestoreOnLoad()
{
	TWeakObjectPtr<USpacePin> WeakThis = this;
	AsyncTask(ENamedThreads::GameThread, [WeakThis]() {
		if (USpacePin* This = WeakThis.Get())
		{
			This->AnchorID = WorldLockingTools::FAlignmentManager::Get()->RestoreAlignmentAnchor(This->AnchorName, This->ModelingPoseGlobal());
			if (This->PinActive())
			{
				FTransform restorePose;
				bool found = WorldLockingTools::FAlignmentManager::Get()->GetAlignmentPose(This->AnchorID, restorePose);
				check(found);
				This->LockedPose = restorePose;
			}
			This->CheckAttachment();
		}
	});
}

void USpacePin::Reset()
{
	// Reset the LockedPose back to identity, when the AlignmentManager resets.
	// This will ensure the old transform does not interfere with tolerance checks in SetLockedPose.
	LockedPose = FTransform::Identity;
}


