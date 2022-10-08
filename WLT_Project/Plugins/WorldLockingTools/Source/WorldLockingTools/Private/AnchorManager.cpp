// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "AnchorManager.h"

#include "IXRTrackingSystem.h"
#include "FrozenWorldInterop.h"

#include "ARBlueprintLibrary.h"
#include "Async/Async.h"

#include "FrozenWorldPlugin.h"
#include "WorldLockingToolsModule.h"

namespace WorldLockingTools
{
	FAnchorManager::FAnchorManager()
	{
		lastAnchorAddTime = std::numeric_limits<float>::min();
		lastTrackingInactiveTime = std::numeric_limits<float>::min();
	}

	/// <summary>
	/// If we have more local anchors than parameterized limit, destroy the furthest.
	/// </summary>
	/// <param name="maxDistAnchorId">Id of the furthest anchor.</param>
	/// <param name="maxDistSpongyAnchor">Reference to the furthest anchor.</param>
	void FAnchorManager::CheckForCull(FrozenWorld_AnchorId maxDistAnchorId, UARPin* maxDistSpongyAnchor)
	{
		/// Anchor limiting is only enabled with a positive limit value.
		if (MaxLocalAnchors > 0)
		{
			if (SpongyAnchors.Num() > MaxLocalAnchors)
			{
				if (maxDistSpongyAnchor != nullptr)
				{
					DestroyAnchor(maxDistAnchorId, maxDistSpongyAnchor);
				}
			}
		}
	}

	/// <summary>
	/// Delete all spongy anchor objects and reset internal state
	/// </summary>
	void FAnchorManager::Reset()
	{
		for(auto anchor : SpongyAnchors)
		{
			DestroyAnchor(FrozenWorld_AnchorId_INVALID, anchor.SpongyAnchor);
		}
		
		SpongyAnchors.Empty();
		FFrozenWorldPlugin::Get()->ClearFrozenAnchors();

		NewSpongyAnchor = DestroyAnchor(FrozenWorld_AnchorId_INVALID, NewSpongyAnchor);
	}

	/// <summary>
	/// Create missing spongy anchors/edges and feed plugin with up-to-date input
	/// </summary>
	/// <returns>Boolean: Has the plugin received input to provide an adjustment?</returns>
	bool FAnchorManager::Update()
	{
		if (GWorld == nullptr)
		{
			return false;
		}

		// To communicate spongyHead and spongyAnchor poses to the FrozenWorld engine, they must all be expressed
		// in the same coordinate system. Here, we do not care where this coordinate
		// system is defined and how it fluctuates over time, as long as it can be used to express the
		// relative poses of all the spongy objects within each time step.
		// 
		FXRHMDData HMDData;
		UHeadMountedDisplayFunctionLibrary::GetHMDData(GWorld, HMDData);

		if (!HMDData.bValid || HMDData.TrackingStatus == ETrackingStatus::NotTracked)
		{
			lastTrackingInactiveTime = GWorld->RealTimeSeconds;
			if (NewSpongyAnchor != nullptr)
			{
				NewSpongyAnchor = DestroyAnchor(FrozenWorld_AnchorId_INVALID, NewSpongyAnchor);
			}
			return false;
		}

		FTransform TrackingToWorld = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(GWorld);
		FTransform WorldToTracking = TrackingToWorld.Inverse();
		FTransform SpongyHead = FTransform(HMDData.Rotation, HMDData.Position) * WorldToTracking;
		FTransform NewSpongyAnchorPose = FTransform(SpongyHead.GetLocation());

		TArray<FrozenWorld_Anchor> ActiveAnchors;
		TArray<FrozenWorld_AnchorId> InnerSphereAnchorIds;
		TArray<FrozenWorld_AnchorId> OuterSphereAnchorIds;

		float MinDistSqr = std::numeric_limits<float>::max();
		FrozenWorld_AnchorId MinDistAnchorId = FrozenWorld_AnchorId_INVALID;

		float maxDistSq = 0;
		FrozenWorld_AnchorId MaxDistAnchorId = FrozenWorld_AnchorId_INVALID;
		UARPin* MaxDistSpongyAnchor = nullptr;

		TArray<FrozenWorld_Edge> NewEdges;
		FrozenWorld_AnchorId NewId = FinalizeNewAnchor(NewEdges);

		float InnerSphereRadSqr = MinNewAnchorDistance * MinNewAnchorDistance;
		float OuterSphereRadSqr = MaxAnchorEdgeLength * MaxAnchorEdgeLength;

		for (const auto& keyval : SpongyAnchors)
		{
			auto id = keyval.AnchorId;
			auto a = keyval.SpongyAnchor;
			if (a->GetTrackingState() == EARTrackingState::Tracking)
			{
				FTransform aSpongyPose = a->GetLocalToTrackingTransform();

				double distSqr = (aSpongyPose.GetLocation() - NewSpongyAnchorPose.GetLocation()).SquaredLength();
				auto anchorPose = FrozenWorld_Anchor{ id, FrozenWorld_FragmentId_UNKNOWN, FFrozenWorldInterop::UtoF(aSpongyPose) };
				ActiveAnchors.Add(anchorPose);
				if (distSqr < MinDistSqr)
				{
					MinDistSqr = distSqr;
					MinDistAnchorId = id;
				}
				if (distSqr <= OuterSphereRadSqr && id != NewId)
				{
					OuterSphereAnchorIds.Add(id);
					if (distSqr <= InnerSphereRadSqr)
					{
						InnerSphereAnchorIds.Add(id);
					}
				}
				if (distSqr > maxDistSq)
				{
					maxDistSq = distSqr;
					MaxDistAnchorId = id;
					MaxDistSpongyAnchor = a;
				}
			}
		}

		if (NewId == 0 && InnerSphereAnchorIds.Num() == 0)
		{
			if (GWorld->RealTimeSeconds <= lastTrackingInactiveTime + TrackingStartDelayTime)
			{
				// Tracking has become active only recently. We suppress creation of new anchors while
				// new anchors may still be in transition due to SpatialAnchor easing.
				//DebugLogExtra($"Skip new anchor creation because only recently gained tracking {Time.unscaledTime - lastTrackingInactiveTime}");
			}
			else if (GWorld->RealTimeSeconds < lastAnchorAddTime + AnchorAddOutTime)
			{
				// short timeout after creating one anchor to prevent bursts of new, unlocatable anchors
				// in case of problems in the anchor generation
				/*DebugLogExtra($"Skip new anchor creation because waiting on recently made anchor "
					+ $"{Time.unscaledTime - lastAnchorAddTime} "
					+ $"- {(newSpongyAnchor != null ? newSpongyAnchor.name : "null")}");*/
			}
			else
			{
				// Unreal expects the anchor pose to be in world space.
				PrepareNewAnchor(NewSpongyAnchorPose * TrackingToWorld, OuterSphereAnchorIds);
				lastAnchorAddTime = GWorld->RealTimeSeconds;
			}
		}

		if (ActiveAnchors.Num() == 0)
		{
			//ErrorStatus = "No active anchors";
			return false;
		}

		// create edges between nearby existing anchors
		if (InnerSphereAnchorIds.Num() >= 2)
		{
			for (const auto& i : InnerSphereAnchorIds)
			{
				if (i != MinDistAnchorId)
				{
					NewEdges.Add(FrozenWorld_Edge{ i, MinDistAnchorId });
				}
			}
		}

		CheckForCull(MaxDistAnchorId, MaxDistSpongyAnchor);

		FFrozenWorldPlugin::Get()->ClearSpongyAnchors();
		FFrozenWorldPlugin::Get()->Step_Init(SpongyHead);
		FFrozenWorldPlugin::Get()->AddSpongyAnchors(ActiveAnchors);
		FFrozenWorldPlugin::Get()->SetMostSignificantSpongyAnchorId(MinDistAnchorId);
		FFrozenWorldPlugin::Get()->AddSpongyEdges(NewEdges);
		FFrozenWorldPlugin::Get()->Step_Finish();

		return true;
	}

	/// <summary>
	/// Load the spongy anchors from persistent storage
	/// 
	/// The set of spongy anchors loaded by this routine is defined by the frozen anchors
	/// previously loaded into the plugin.
	/// 
	/// Likewise, when a spongy anchor fails to load, this routine will delete its frozen
	/// counterpart from the plugin.
	/// </summary>
	void FAnchorManager::LoadAnchors()
	{
		// Wait for ARSession to start and anchor store to be ready.
		// This is called from a background thread, so we can loop until the anchor store is ready.
		while (UARBlueprintLibrary::GetARSessionStatus().Status != EARSessionStatus::Running)
		{
			// Wait for ARSession to start
			FPlatformProcess::Sleep(0.1f);
		}

		if (!UARBlueprintLibrary::IsARPinLocalStoreSupported())
		{
			return;
		}

		while (!UARBlueprintLibrary::IsARPinLocalStoreReady())
		{
			FPlatformProcess::Sleep(0.1f);
		}

		AsyncTask(ENamedThreads::GameThread, [this]() {
			auto anchorIds = FFrozenWorldPlugin::Get()->GetFrozenAnchorIds();

			FrozenWorld_AnchorId maxId = NewAnchorId;

			TMap<FName, UARPin*> AnchorMap = UARBlueprintLibrary::LoadARPinsFromLocalStore();
			for (const auto& id : anchorIds)
			{
				FName AnchorName = FName("FW_Anchor_" + FString::FromInt((int)id));
				if (AnchorMap.Contains(AnchorName))
				{
					anchorsByTrackableId.Add(id, AnchorMap[AnchorName]);
					SpongyAnchors.Add(
						SpongyAnchorWithId
						{
							id,
							AnchorMap[AnchorName]
						}
					);
				}
				else
				{
					FFrozenWorldPlugin::Get()->RemoveFrozenAnchor(id);
				}
			}

			for (const auto& spongyAnchor : SpongyAnchors)
			{
				if (NewAnchorId <= spongyAnchor.AnchorId)
				{
					NewAnchorId = spongyAnchor.AnchorId + 1;
				}
			}
		});
	}

	/// <summary>
	/// Platform dependent instantiation of a local anchor at given position.
	/// </summary>
	/// <param name="id">Anchor id to give new anchor.</param>
	/// <param name="AnchorSceneComponent">Object to hang anchor off of.</param>
	/// <param name="initialPose">Pose for the anchor.</param>
	/// <returns>The new anchor</returns>
	UARPin* FAnchorManager::CreateAnchor(FrozenWorld_AnchorId id, USceneComponent* AnchorSceneComponent, FTransform initialPose)
	{
		UE_LOG(LogWLT, Log, TEXT("Creating anchor %d"), id);

		UARPin* Pin = UARBlueprintLibrary::PinComponent(AnchorSceneComponent, initialPose);
		anchorsByTrackableId.Add(id, Pin);

		FName AnchorName = FName("FW_Anchor_" + FString::FromInt((int)id));
		UARBlueprintLibrary::RemoveARPinFromLocalStore(AnchorName);
		UARBlueprintLibrary::SaveARPinToLocalStore(AnchorName, Pin);

		return Pin;
	}

	/// <summary>
	/// Dispose local anchor.
	/// The id is used to delete from any stored lists. If the SpongyAnchor hasn't been
	/// added to any lists (is still initializing), id can be AnchorId_Invalid.
	/// </summary>
	/// <param name="id">The id of the anchor to destroy.</param>
	/// <param name="spongyAnchor">Reference to the anchor to destroy.</param>
	/// <returns>Null</returns>
	UARPin* FAnchorManager::DestroyAnchor(FrozenWorld_AnchorId id, UARPin* spongyAnchor)
	{
		UE_LOG(LogWLT, Log, TEXT("Destroying anchor %d"), id);

		if (spongyAnchor != nullptr && anchorsByTrackableId.Contains(id))
		{
			FName AnchorName = FName("FW_Anchor_" + FString::FromInt((int)id));
			UARBlueprintLibrary::RemoveARPinFromLocalStore(AnchorName);

			UARBlueprintLibrary::RemovePin(spongyAnchor);

			anchorsByTrackableId.Remove(id);
		}

		if (id != FrozenWorld_AnchorId_INVALID && id != FrozenWorld_AnchorId_UNKNOWN)
		{
			FFrozenWorldPlugin::Get()->RemoveFrozenAnchor(id);

			int index = 0;
			for (const auto& entry : SpongyAnchors)
			{
				if (entry.AnchorId == id)
				{
					SpongyAnchors.RemoveAt(index);
					break;
				}

				index++;
			}
		}

		return nullptr;
	}

	/// <summary>
	/// prepare potential new anchor, which will only be finalized in a later time step
	/// when isLocated is actually found to be true.
	/// </summary>
	void FAnchorManager::PrepareNewAnchor(FTransform pose, TArray<FrozenWorld_AnchorId> neighbors)
	{
		if (NewSpongyAnchor != nullptr)
		{
			//DebugLogExtra($"Discarding {newSpongyAnchor.name} (located={newSpongyAnchor.IsLocated}) because still not located");
			NewSpongyAnchor = DestroyAnchor(FrozenWorld_AnchorId_INVALID, NewSpongyAnchor);
		}

		USceneComponent* AnchorSceneComponent = NewObject<USceneComponent>();
		AnchorSceneComponent->AddToRoot();

		NewSpongyAnchor = CreateAnchor(NextAnchorId(), AnchorSceneComponent, pose);
		NewAnchorNeighbors = neighbors;
	}

	/// <summary>
	/// If a potential new anchor was prepared (in a previous time step) and is now found to be
	/// located, this routine finalizes it and prepares its edges to be added
	/// </summary>
	/// <param name="OutNewEdges">List that will have new edges appended by this routine</param>
	/// <returns>new anchor id (or Invalid if none was finalized)</returns>
	FrozenWorld_AnchorId FAnchorManager::FinalizeNewAnchor(TArray<FrozenWorld_Edge>& OutNewEdges)
	{
		if (NewSpongyAnchor == nullptr || NewSpongyAnchor->GetTrackingState() != EARTrackingState::Tracking)
		{
			return FrozenWorld_AnchorId_INVALID;
		}

		FrozenWorld_AnchorId NewId = ClaimAnchorId();
		for (const auto& id : NewAnchorNeighbors)
		{
			OutNewEdges.Add(FrozenWorld_Edge{ id, NewId });
		}

		SpongyAnchors.Add(
			SpongyAnchorWithId
			{
				NewId,
				std::move(NewSpongyAnchor)
			}
		);
		NewSpongyAnchor = nullptr;

		return NewId;
	}

	/// <summary>
	/// Return the next available anchor id.
	/// 
	/// This function doesn't claim the id, only returns what the next will be.
	/// Use ClaimAnchorId() to obtain the next id and keep any other caller from claiming it.
	/// </summary>
	/// <returns>Next available id</returns>
	FrozenWorld_AnchorId FAnchorManager::NextAnchorId()
	{
		return NewAnchorId;
	}

	/// <summary>
	/// Claim a unique anchor id.
	/// </summary>
	/// <returns>The exclusive anchor id</returns>
	FrozenWorld_AnchorId FAnchorManager::ClaimAnchorId()
	{
		return NewAnchorId++;
	}
}