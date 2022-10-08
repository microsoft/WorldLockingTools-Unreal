// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "FragmentManager.h"

#include "AttachmentPoint.h"
#include "FrozenWorldPlugin.h"

#include "WorldLockingToolsModule.h"

namespace WorldLockingTools
{
	FFragmentManager::FFragmentManager()
	{
		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
	}

	FFragmentManager::~FFragmentManager()
	{
		IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
	}

	FFragmentManager* FFragmentManager::Get()
	{
		return &IModularFeatures::Get().GetModularFeature<FFragmentManager>(GetModularFeatureName());
	}

	/// <summary>
	/// Set all fragments unconnected during a temporary system outage, especially
	/// while tracking is lost.
	/// 
	/// Fragments to resume as they were on next update. Pause may be called multiple 
	/// consecutive frames, as long as the system outage continues, but only Pause or 
	/// Update should be called on a given frame.
	/// </summary>
	void FFragmentManager::Pause()
	{
		if (CurrentFragmentId != FrozenWorld_FragmentId_INVALID)
		{
			CurrentFragmentId = FrozenWorld_FragmentId_INVALID;
			ApplyActiveCurrentFragment();
		}
	}

	/// <summary>
	/// Perform any pending refit operations and reconcile state accordingly.
	/// </summary>
	/// <param name="autoRefreeze">True to automatically perform a refreeze if indicated by the plugin.</param>
	/// <param name="autoMerge">True to automatically perform a merge if indicated by the plugin.</param>
	void FFragmentManager::Update(bool autoRefreeze, bool autoMerge)
	{
		CurrentFragmentId = FFrozenWorldPlugin::Get()->GetMostSignificantFragmentId();
		
		if (CurrentFragmentId == FrozenWorld_FragmentId_UNKNOWN ||
			CurrentFragmentId == FrozenWorld_FragmentId_INVALID)
		{
			check(false) // Update shouldn't be called with no active fragment.
			return;
		}
		EnsureFragment(CurrentFragmentId);

		if (FFrozenWorldPlugin::Get()->GetMetrics().refitRefreezeIndicated && autoRefreeze)
		{
			Refreeze();
		}
		else if (FFrozenWorldPlugin::Get()->GetMetrics().refitMergeIndicated && autoMerge)
		{
			// there are multiple mergeable fragments -- do the merge and show the result
			Merge();
		}

		// There are multiple mergeable fragments with valid adjustments, but we show only the current one
		ApplyActiveCurrentFragment();

		ProcessPendingAttachmentPoints();
	}

	/// <summary>
	/// Clear all internal state and resources.
	/// </summary>
	void FFragmentManager::Reset()
	{
		fragments.Empty();
		CurrentFragmentId = FrozenWorld_FragmentId_INVALID;
		
		TArray<FrozenWorld_FragmentId> empty;
		refitNotifications.ExecuteIfBound(FrozenWorld_FragmentId_INVALID, empty);
	}

	/// <summary>
	/// If conditions have changed to allow finalizing creation of any pending attachment points, do it now.
	/// </summary>
	void FFragmentManager::ProcessPendingAttachmentPoints()
	{
		if (CurrentFragmentId != FrozenWorld_FragmentId_UNKNOWN
			&& CurrentFragmentId != FrozenWorld_FragmentId_INVALID
			&& pendingAttachments.Num() > 0)
		{
			// We have a valid destination fragment. Note that since this queue is in order of submission,
			// if an attachment point depends on a second attachment point for context,
			// that second will be either earlier in the list (because there was no valid current fragment when it was
			// created) or it will have a valid fragment. So by the time we get to the one with a dependency (pending.context != null),
			// its dependency will have a valid fragment id.
			int pendingCount = pendingAttachments.Num();
			for (int i = 0; i < pendingCount; ++i)
			{
				TSharedPtr<FAttachmentPoint> target = pendingAttachments[i].target;
				FVector frozenPosition = target->ObjectPosition;
				TSharedPtr<FAttachmentPoint> context = pendingAttachments[i].context;

				SetupAttachmentPoint(target, context);

				FrozenWorld_FragmentId fragmentId = CurrentFragmentId;
				if (context != nullptr)
				{
					fragmentId = context->FragmentId;
				}
				check(fragmentId != FrozenWorld_FragmentId_UNKNOWN && fragmentId != FrozenWorld_FragmentId_INVALID); // Invalid fragmentId
				TSharedPtr<FFragment> fragment = EnsureFragment(fragmentId);
				check(fragment != nullptr); //Valid fragmentId but no fragment found.
				fragment->AddAttachmentPoint(target);
			}

			// All pending must now be in a good home fragment, clear the to-do list.
			pendingAttachments.Empty();
		}
	}

	/// <summary>
	/// Helper function for setting up the internals of an AttachmentPoint
	/// </summary>
	/// <param name="target">The attachment point to setup</param>
	/// <param name="context">The optional context <see cref="CreateAttachmentPoint"/></param>
	void FFragmentManager::SetupAttachmentPoint(TSharedPtr<FAttachmentPoint> target, TSharedPtr<FAttachmentPoint> context)
	{
		if (context != nullptr)
		{
			FrozenWorld_AnchorId anchorId;
			FVector locationFromAnchor;
			
			FFrozenWorldPlugin::Get()->CreateAttachmentPointFromSpawner(context->AnchorId, context->LocationFromAnchor,
				target->ObjectPosition, anchorId, locationFromAnchor);
			FrozenWorld_FragmentId fragmentId = context->FragmentId;
			target->Set(fragmentId, target->ObjectPosition, anchorId, locationFromAnchor);
		}
		else
		{
			FrozenWorld_FragmentId currentFragmentId = FFrozenWorldPlugin::Get()->GetMostSignificantFragmentId();
			FrozenWorld_AnchorId anchorId;
			FVector locationFromAnchor;
			FFrozenWorldPlugin::Get()->CreateAttachmentPointFromHead(target->ObjectPosition, anchorId, locationFromAnchor);
			FrozenWorld_FragmentId fragmentId = currentFragmentId;
			target->Set(fragmentId, target->ObjectPosition, anchorId, locationFromAnchor);
		}
	}

	/// <summary>
	/// Add a new attachment point to the pending list to be processed when the system is ready.
	/// </summary>
	/// <param name="attachPoint">Attachment point to process later.</param>
	/// <param name="context">Optional spawning attachment point, may be null.</param>
	void FFragmentManager::AddPendingAttachmentPoint(TSharedPtr<FAttachmentPoint> attachPoint, TSharedPtr<FAttachmentPoint> context)
	{
		attachPoint->HandleStateChange(AttachmentPointStateType::Pending);

		pendingAttachments.Add(
			PendingAttachmentPoint
			{
				attachPoint,
				context
			}
		);
	}

	/// <summary>
	/// Create and register a new attachment point.
	/// 
	/// The attachment point itself is a fairly opaque handle. Its effects are propagated to the client via the
	/// two handlers associated with it.
	/// The optional context attachment point provides an optional contextual hint to where in the anchor
	/// graph to bind the new attachment point.
	/// See FAttachmentPointManager::CreateAttachmentPoint.
	/// </summary>
	/// <param name="frozenPosition">The position in the frozen space at which to start the attachment point</param>
	/// <param name="context">The optional context into which to create the attachment point (may be null)</param>
	/// <param name="locationHandler">Delegate to handle WorldLocking system adjustments to position</param>
	/// <param name="stateHandler">Delegate to handle WorldLocking connectivity changes</param>
	/// <returns>The new attachment point interface.</returns>
	TSharedPtr<FAttachmentPoint> FFragmentManager::CreateAttachmentPoint(
		FVector frozenPosition, 
		TSharedPtr<FAttachmentPoint> context,
		FAttachmentPoint::FAdjustLocationDelegate LocationHandler,
		FAttachmentPoint::FAdjustStateDelegate StateHandler)
	{
		FrozenWorld_FragmentId fragmentId = GetTargetFragmentId(context);
		TSharedPtr<FAttachmentPoint> attachPoint = MakeShared<FAttachmentPoint>(LocationHandler, StateHandler);

		attachPoint->ObjectPosition = frozenPosition;
		if (fragmentId != FrozenWorld_FragmentId_UNKNOWN 
			&& fragmentId != FrozenWorld_FragmentId_INVALID)
		{
			SetupAttachmentPoint(attachPoint, context);

			TSharedPtr<FFragment> fragment = EnsureFragment(fragmentId);
			check(fragment != nullptr) //Valid fragmentId but no fragment found
			fragment->AddAttachmentPoint(attachPoint);
		}
		else
		{
			AddPendingAttachmentPoint(attachPoint, context);
		}
		return attachPoint;
	}

	/// <summary>
	/// Teleport (as opposed to Move) means that the object is meant to have disappeared at its old position 
	/// and instantaneously reappeared at its new position in frozen space without traversing the space in between.
	/// 
	/// This is equivalent to releasing the existing attachment point and creating a new one,
	/// except in that the attachment point reference remains valid.
	/// </summary>
	/// <param name="attachPointIface">The attachment point to teleport</param>
	/// <param name="newFrozenPosition">The position to teleport to.</param>
	/// <param name="context">The optional context.</param>
	void FFragmentManager::TeleportAttachmentPoint(TSharedPtr<FAttachmentPoint> attachPointIface, FVector newFrozenPosition, TSharedPtr<FAttachmentPoint> context)
	{
		if (attachPointIface != nullptr)
		{
			attachPointIface->ObjectPosition = newFrozenPosition;

			// Save the fragment it's currently in, in case it changes here.
			FrozenWorld_FragmentId oldFragmentId = attachPointIface->FragmentId;

			// If it's not in a valid fragment, it is still pending and will get processed when the system is ready.
			if (oldFragmentId != FrozenWorld_FragmentId_UNKNOWN
				&& oldFragmentId != FrozenWorld_FragmentId_INVALID)
			{
				FrozenWorld_FragmentId newFragmentId = GetTargetFragmentId(context);
				// If there is a valid current fragment, 
				if (newFragmentId != FrozenWorld_FragmentId_UNKNOWN
					&& newFragmentId != FrozenWorld_FragmentId_INVALID)
				{
					// Fill it in with a new one.
					SetupAttachmentPoint(attachPointIface, context);

					if (attachPointIface->FragmentId != oldFragmentId)
					{
						ChangeAttachmentPointFragment(oldFragmentId, attachPointIface);
					}
				}
				else
				{
					AddPendingAttachmentPoint(attachPointIface, context);
				}
			}
		}
	}

	/// <summary>
	/// Release an attachment point for disposal. The attachment point is no longer valid after this call.
	/// 
	/// In the unlikely circumstance that another attachment point has been spawned from this one
	/// but has not yet been processed (is still in the pending queue),
	/// that relationship is broken on release of this one, and when the other attachment point is
	/// finally processed, it will be as if it was created with a null context.
	/// </summary>
	/// <param name="attachPointIface">The attachment point to release.</param>
	void FFragmentManager::ReleaseAttachmentPoint(TSharedPtr<FAttachmentPoint> AttachmentPoint)
	{
		if (AttachmentPoint != nullptr)
		{
			TSharedPtr<FFragment> fragment = EnsureFragment(AttachmentPoint->FragmentId);
			if (fragment != nullptr)
			{
				// Fragment handles notification.
				fragment->ReleaseAttachmentPoint(AttachmentPoint);
			}
			else
			{
				// Notify of the state change to released.
				AttachmentPoint->HandleStateChange(AttachmentPointStateType::Released);
				// The list of pending attachments is expected to be small, and release of an attachment
				// point while there are pending attachments is expected to be rare. So brute force it here.
				// If the attachment point being released is a target in the pending list, remove it.
				// If it is the context of another pending target, set that context to null.
				// Proceed through the list in reverse order, because context fixes will only be found
				// later in the list than the original, and once the original is found we are done.
				int pendingCount = pendingAttachments.Num();
				for (int i = pendingCount - 1; i >= 0; --i)
				{
					if (pendingAttachments[i].context == AttachmentPoint)
					{
						auto p = pendingAttachments[i];
						p.context = nullptr;
						pendingAttachments[i] = p;
					}
					else if (pendingAttachments[i].target == AttachmentPoint)
					{
						pendingAttachments.RemoveAt(i);
						break;
					}
				}
			}
		}
	}

	/// <summary>
	/// Establish which fragment a new attachment point should join.
	/// </summary>
	/// <param name="context">Optional spawning attachment point. May be null to "spawn from head".</param>
	/// <returns>Id of fragment to join. May be FragmentId_Invalid if not currently tracking.</returns>
	FrozenWorld_FragmentId FFragmentManager::GetTargetFragmentId(TSharedPtr<FAttachmentPoint> context)
	{
		FrozenWorld_FragmentId fragmentId = CurrentFragmentId;
		if (context != nullptr)
		{
			fragmentId = context->FragmentId;
		}
		return fragmentId;
	}

	/// <summary>
	/// Helper to move an attachment point from one fragment to another.
	/// 
	/// Assumes that the attachment point's FragmentId property has already been set to the new fragment.
	/// </summary>
	/// <param name="oldFragmentId">Source fragment</param>
	/// <param name="attachPoint">The attachment point</param>
	void FFragmentManager::ChangeAttachmentPointFragment(FrozenWorld_FragmentId oldFragmentId, TSharedPtr<FAttachmentPoint> attachPoint)
	{
		check(oldFragmentId != attachPoint->FragmentId); //Moving attachment point from and to same fragment

		TSharedPtr<FFragment> oldFragment = EnsureFragment(oldFragmentId);
		TSharedPtr<FFragment> newFragment = EnsureFragment(attachPoint->FragmentId);
		check(oldFragment != nullptr); //Valid fragmentId's but null source fragment
		check(newFragment != nullptr); //Valid fragmentId's but null destination fragment

		// Add to the new fragment
		newFragment->AddAttachmentPoint(attachPoint);

		// Remove from the old fragment
		oldFragment->ReleaseAttachmentPoint(attachPoint);
	}

	/// <summary>
	/// Check existence of fragment with indicated id, 
	/// and create it if it doesn't already exist.
	/// </summary>
	/// <param name="id">The fragment id</param>
	TSharedPtr<FFragment> FFragmentManager::EnsureFragment(FrozenWorld_FragmentId id)
	{
		if (id == FrozenWorld_FragmentId_UNKNOWN ||
			id == FrozenWorld_FragmentId_INVALID)
		{
			return nullptr;
		}
		
		if (!fragments.Contains(id))
		{
			fragments.Add(id, MakeShared<FFragment>(id));
		}
		return fragments[id];
	}

	/// <summary>
	/// Notify all fragments of their current state.
	/// </summary>
	void FFragmentManager::ApplyActiveCurrentFragment()
	{
		for (const auto& fragment : fragments)
		{
			AttachmentPointStateType state = fragment.Value->FragmentId == CurrentFragmentId
				? AttachmentPointStateType::Normal
				: AttachmentPointStateType::Unconnected;
			
			fragment.Value->UpdateState(state);
		}
	}

	/// <summary>
	/// Call on the plugin to compute the merge, then apply by
	/// setting transforms and adjusting scene graph.
	/// </summary>
	/// <returns>True for successful merge.</returns>
	bool FFragmentManager::Merge()
	{
		FrozenWorld_FragmentId targetFragmentId;
		TArray<FragmentPose> mergeAdjustments;
		if (!FFrozenWorldPlugin::Get()->Merge(targetFragmentId, mergeAdjustments))
		{
			return false;
		}

		//Received invalid merged fragment id from successful merge
		check(targetFragmentId != FrozenWorld_FragmentId_INVALID && targetFragmentId != FrozenWorld_FragmentId_UNKNOWN);

		TSharedPtr<FFragment> targetFragment = EnsureFragment(targetFragmentId);
		check(targetFragment != nullptr); //Valid fragmentId but null target fragment from Merge.

		if (targetFragment == nullptr)
		{
			return false;
		}

		int numAbsorbed = mergeAdjustments.Num();
		for (int i = 0; i < numAbsorbed; ++i)
		{
			FrozenWorld_FragmentId sourceId = mergeAdjustments[i].fragmentId;
			FTransform adjustment = mergeAdjustments[i].pose;
			if (fragments.Contains(sourceId))
			{
				targetFragment->AbsorbOtherFragment(*fragments[sourceId], adjustment);
				fragments.Remove(sourceId);
			}
			else
			{
				UE_LOG(LogWLT, Error, TEXT("Try to merge in a non-existent fragment %d"), sourceId);
			}
		}
		CurrentFragmentId = targetFragmentId;

		ApplyActiveCurrentFragment();

		refitNotifications.ExecuteIfBound(targetFragment->FragmentId, ExtractFragmentIds(mergeAdjustments));

		return true;
	}

	TArray<FrozenWorld_FragmentId> FFragmentManager::ExtractFragmentIds(TArray<FragmentPose> source)
	{
		TArray<FrozenWorld_FragmentId> ids;
		for (int i = 0; i < source.Num(); ++i)
		{
			ids.Add(source[i].fragmentId);
		}
		return ids;
	}

	/// <summary>
	/// Invoke a refreeze operation on the plugin, and make all necessary adjustments
	/// in bookeeping after.
	/// </summary>
	/// <returns>True for successful refreeze.</returns>
	bool FFragmentManager::Refreeze()
	{
		FrozenWorld_FragmentId targetFragmentId;
		TArray<FrozenWorld_FragmentId> absorbedIds;
		if (!FFrozenWorldPlugin::Get()->Refreeze(targetFragmentId, absorbedIds))
		{
			return false;
		}
		// Received invalid merged fragment id from successful refreeze.
		check(targetFragmentId != FrozenWorld_FragmentId_INVALID && targetFragmentId != FrozenWorld_FragmentId_UNKNOWN);

		TSharedPtr<FFragment> targetFragment = EnsureFragment(targetFragmentId);
		check(targetFragment != nullptr); //Valid fragmentId but no fragment found

		if (targetFragment == nullptr)
		{
			return false;
		}

		for (int i = 0; i < absorbedIds.Num(); ++i)
		{
			FrozenWorld_FragmentId sourceId = absorbedIds[i];
			if (sourceId != targetFragmentId)
			{
				if (fragments.Contains(sourceId))
				{
					targetFragment->AbsorbOtherFragment(*fragments[sourceId]);
					fragments.Remove(sourceId);
				}
				else
				{
					UE_LOG(LogWLT, Error, TEXT("Try to merge in a non-existent fragment %d"), sourceId);
				}
			}
		}
		CurrentFragmentId = targetFragmentId;

		// now apply individual adjustments to each attachment point.
		targetFragment->AdjustAll();

		// now that all adjustments have been made, notify the plugin to finish up the operation.
		FFrozenWorldPlugin::Get()->RefreezeFinish();

		check(IsInGameThread());
		refitNotifications.ExecuteIfBound(targetFragment->FragmentId, absorbedIds);

		return true;
	}
}