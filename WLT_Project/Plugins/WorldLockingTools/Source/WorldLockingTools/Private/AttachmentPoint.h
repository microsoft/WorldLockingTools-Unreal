// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#pragma warning(push)
#pragma warning(disable: 4996)
#include "FrozenWorldEngine.h"
#pragma warning(pop)

#include "CoreMinimal.h"

namespace WorldLockingTools
{
	/// <summary>
	/// The states an attachment point can be in.
	/// </summary>
	enum AttachmentPointStateType
	{
		Invalid = 0,	// Doesn't exist
		Pending,		// Exists, but is still under construction
		Normal,		 // Exists, and is active and valid
		Unconnected,	// Exists, but is disconnected from the active fragment. Location data unreliable.
		Released,	   // Existed, but has been released. Is now garbage.
	};

	/// <summary>
	/// Opaque handle to an attachment point. Create one of these to enable
	/// WorldLocking to adjust an attached object as corrections to the world locked space 
	/// optimization are made. 
	///
	/// The attachment point gives an interface for notifying the system that you have moved
	/// the attached object, and the system indicates that it has computed an adjustment
	/// for the object through the callbacks passed into the creation routine.
	/// Alternatively, polling is also supported through the State and ObjectAdjustment accessors.
	/// </summary>
	class FAttachmentPoint
	{
	public:
		DECLARE_DELEGATE_OneParam(FAdjustLocationDelegate, FTransform);
		FAdjustLocationDelegate LocationHandler;

		DECLARE_DELEGATE_OneParam(FAdjustStateDelegate, AttachmentPointStateType);
		FAdjustStateDelegate StateHandler;

	public:
		bool operator == (FAttachmentPoint other)
		{
			return AnchorId == other.AnchorId && FragmentId == other.FragmentId;
		}

		FAttachmentPoint(FAdjustLocationDelegate InLocationHandler, FAdjustStateDelegate InStateHandler) :
			LocationHandler(InLocationHandler),
			StateHandler(InStateHandler)
		{
		}

	public:
		FrozenWorld_AnchorId AnchorId;
		FrozenWorld_FragmentId FragmentId;
		// Position of attachment point in anchor point's space.
		FVector LocationFromAnchor;
		// Internal history cache.
		FVector CachedPosition;
		// Current state of this attachment point. 
		// Positioning information is only valid when state is Normal.
		AttachmentPointStateType State;
		// Cumulative transform adjustment for object(s) bound to this attachment point.
		FTransform ObjectAdjustment;
		// The position of object(s) bound to this attachment point.
		FVector ObjectPosition;

		void Set(FrozenWorld_FragmentId fragmentId, FVector cachedPosition, FrozenWorld_AnchorId anchorId, FVector locationFromAnchor);

		void HandleStateChange(AttachmentPointStateType newState);

		void HandlePoseAdjustment(FTransform adjustment);
	};
}