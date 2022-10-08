// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "Fragment.h"
#include "FrozenWorldInterop.h"
#include "FrozenWorldPlugin.h"

#include "WorldLockingToolsModule.h"

namespace WorldLockingTools
{
	FFragment::FFragment(FrozenWorld_FragmentId fragmentId) :
		FragmentId(fragmentId)
	{
	}

	/// <summary>
	/// Set the state of the contents of this fragment.
	/// </summary>
	void FFragment::UpdateState(AttachmentPointStateType attachmentState)
	{
		check(IsInGameThread());
		if (State != attachmentState)
		{
			State = attachmentState;

			for (const auto& updateStateAttachment : updateStateAllAttachments)
			{
				if (updateStateAttachment.IsBound())
				{
					updateStateAttachment.ExecuteIfBound(attachmentState);
				}
			}
		}
	}

	/// <summary>
	/// Add an existing attachment point to this fragment.
	/// 
	/// The attachment point might currently belong to another fragment, if
	/// it is being moved from the other to this.
	/// Since this is only used internally, it operates directly on an AttachmentPoint
	/// rather than an interface to avoid an unnecessary downcast.
	/// </summary>
	void FFragment::AddAttachmentPoint(TSharedPtr<FAttachmentPoint> attachPoint)
	{
		if (attachPoint->StateHandler.IsBound())
		{
			updateStateAllAttachments.Add(attachPoint->StateHandler);
		}

		attachPoint->HandleStateChange(State);
		attachmentList.Add(attachPoint);
	}

	/// <summary>
	/// Notify system attachment point is no longer needed. See FAttachmentPointManager::ReleaseAttachmentPoint
	/// </summary>
	void FFragment::ReleaseAttachmentPoint(TSharedPtr<FAttachmentPoint> attachmentPoint)
	{
		if (attachmentPoint != nullptr)
		{
			if (attachmentPoint->StateHandler.IsBound())
			{
				attachmentPoint->StateHandler.Unbind();
			}
			attachmentPoint->HandleStateChange(AttachmentPointStateType::Released);
			attachmentList.Remove(attachmentPoint);
		}
		else
		{
			UE_LOG(LogWLT, Error, TEXT("On release, IAttachmentPoint isn't AttachmentPoint."));
		}
	}

	/// <summary>
	/// Release all resources for this fragment.
	/// </summary>
	void FFragment::ReleaseAll()
	{
		updateStateAllAttachments.Empty();
		attachmentList.Empty();
	}

	/// <summary>
	/// Absorb the contents of another fragment, emptying it.
	/// </summary>
	/// <param name="other">The fragment to lose all its contents to this.</param>
	void FFragment::AbsorbOtherFragment(FFragment other)
	{
		check(&other != this); // Trying to merge to and from the same fragment
		int otherCount = other.attachmentList.Num();
		for (int i = 0; i < otherCount; ++i)
		{
			TSharedPtr<FAttachmentPoint> att = other.attachmentList[i];
			att->Set(FragmentId, att->CachedPosition, att->AnchorId, att->LocationFromAnchor);
			if (att->StateHandler.IsBound())
			{
				updateStateAllAttachments.Add(att->StateHandler);
			}
			att->HandleStateChange(State);
			attachmentList.Add(att);
		}
		other.ReleaseAll();
	}

	/// <summary>
	/// Absorb the contents of another fragment, emptying it, and applying an adjustment transform.
	/// </summary>
	/// <param name="other">The fragment to lose all its contents to this.</param>
	/// <param name="adjustment">Pose adjustment to apply to contents of other on transition.</param>
	void FFragment::AbsorbOtherFragment(FFragment other, FTransform adjustment)
	{
		check(&other != this); // Trying to merge to and from the same fragment
		int otherCount = other.attachmentList.Num();
		for (int i = 0; i < otherCount; ++i)
		{
			TSharedPtr<FAttachmentPoint> att = other.attachmentList[i];
			att->Set(FragmentId, att->CachedPosition, att->AnchorId, att->LocationFromAnchor);
			att->HandlePoseAdjustment(adjustment);
			if (att->StateHandler.IsBound())
			{
				updateStateAllAttachments.Add(att->StateHandler);
			}
			att->HandleStateChange(State);
			attachmentList.Add(att);
		}
		other.ReleaseAll();
	}

	/// <summary>
	/// Run through all attachment points, get their adjustments from the plugin and apply them.
	/// 
	/// This must be called between FFrozenWorldPlugin::Refreeze() and FFrozenWorldPlugin::RefreezeFinish().
	/// </summary>
	void FFragment::AdjustAll()
	{
		int count = attachmentList.Num();
		for (int i = 0; i < count; ++i)
		{
			TSharedPtr<FAttachmentPoint> attach = attachmentList[i];

			FrozenWorld_AnchorId newAnchorId;
			FVector newLocationFromAnchor;
			FTransform adjustment;
			if (FFrozenWorldPlugin::Get()->ComputeAttachmentPointAdjustment(attach->AnchorId, attach->LocationFromAnchor,
				newAnchorId, newLocationFromAnchor, adjustment))
			{
				attach->Set(FragmentId, attach->CachedPosition, newAnchorId, newLocationFromAnchor);

				attach->HandlePoseAdjustment(adjustment);
			}
			else
			{
				UE_LOG(LogWLT, Warning, TEXT("No adjustment during refreeze for %d"), attach->AnchorId);
			}
		}
	}
}