// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "AttachmentPoint.h"
#include "FrozenWorldPoseExtensions.h"

namespace WorldLockingTools
{
	/// <summary>
	/// Set internals of attachment point to new values.
	/// </summary>
	/// <param name="fragmentId">New fragment</param>
	/// <param name="cachedPosition">Cache last position moved to.</param>
	/// <param name="anchorId">New anchor id</param>
	/// <param name="locationFromAnchor">New displacement from anchor</param>
	void FAttachmentPoint::Set(FrozenWorld_FragmentId fragmentId, FVector cachedPosition, FrozenWorld_AnchorId anchorId, FVector locationFromAnchor)
	{
		AnchorId = anchorId;
		FragmentId = fragmentId;
		CachedPosition = cachedPosition;
		LocationFromAnchor = locationFromAnchor;
	}

	/// <summary>
	/// If state has changed, record the new state and pass on to client handler (if any).
	/// </summary>
	/// <param name="newState">The state to change to.</param>
	void FAttachmentPoint::HandleStateChange(AttachmentPointStateType newState)
	{
		check(IsInGameThread());

		if (newState != State)
		{
			State = newState;
			StateHandler.ExecuteIfBound(newState);
		}
	}

	/// <summary>
	/// Keep track of cumulative transform adjustment, and pass on to client adjustment handler (if any).
	/// </summary>
	void FAttachmentPoint::HandlePoseAdjustment(FTransform adjustment)
	{
		check(IsInGameThread());

		ObjectPosition = FFrozenWorldPoseExtensions::Multiply(adjustment, ObjectPosition);
		ObjectAdjustment = FFrozenWorldPoseExtensions::Multiply(ObjectAdjustment, adjustment);

		LocationHandler.ExecuteIfBound(adjustment);
	}
}