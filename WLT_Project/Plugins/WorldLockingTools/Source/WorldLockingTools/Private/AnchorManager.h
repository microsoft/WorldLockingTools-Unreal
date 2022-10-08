// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "ARPin.h"
#include "HeadMountedDisplayFunctionLibrary.h"

#pragma warning(push)
#pragma warning(disable: 4996)
#include "FrozenWorldEngine.h"
#pragma warning(pop)

namespace WorldLockingTools
{
	struct SpongyAnchorWithId
	{
		FrozenWorld_AnchorId AnchorId;
		UARPin* SpongyAnchor;
	};

	class FAnchorManager
	{
	public:
		float MinNewAnchorDistance = 100.0f;
		float MaxAnchorEdgeLength = 120.0f;
		// 0 indicates unlimited anchors
		int MaxLocalAnchors = 0;

		float TrackingStartDelayTime = 0.3f;
		float AnchorAddOutTime = 0.4f;

	private:
		static inline FrozenWorld_AnchorId NewAnchorId = FrozenWorld_AnchorId_INVALID + 1;
		UARPin* NewSpongyAnchor = nullptr;
		TArray<FrozenWorld_AnchorId> NewAnchorNeighbors;
		TArray<SpongyAnchorWithId> SpongyAnchors;

		float lastAnchorAddTime;
		float lastTrackingInactiveTime;

		TMap<FrozenWorld_AnchorId, UARPin*> anchorsByTrackableId;

	public:
		FAnchorManager();

		void Reset();
		bool Update();

		void LoadAnchors();

	private:
		UARPin* CreateAnchor(FrozenWorld_AnchorId id, USceneComponent* AnchorSceneComponent, FTransform initialPose);
		UARPin* DestroyAnchor(FrozenWorld_AnchorId id, UARPin* spongyAnchor);

		void PrepareNewAnchor(FTransform pose, TArray<FrozenWorld_AnchorId> neighbors);
		FrozenWorld_AnchorId FinalizeNewAnchor(TArray<FrozenWorld_Edge>& OutNewEdges);

		void CheckForCull(FrozenWorld_AnchorId maxDistAnchorId, UARPin* maxDistSpongyAnchor);

		FrozenWorld_AnchorId NextAnchorId();
		FrozenWorld_AnchorId ClaimAnchorId();
	};
}