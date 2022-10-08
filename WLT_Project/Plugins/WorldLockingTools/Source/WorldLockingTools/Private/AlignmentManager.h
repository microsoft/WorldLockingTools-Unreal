// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#pragma warning(push)
#pragma warning(disable: 4996)
#include "FrozenWorldEngine.h"
#pragma warning(pop)

#include "AttachmentPoint.h"
#include "Triangulator.h"

#include "Delegates/Delegate.h"
#include "Features/IModularFeatures.h"

namespace WorldLockingTools
{
	/// <summary>
	/// The final pose with a single weight.
	/// 
	/// Note that this weight is not normalized in the sense of the weights summing to one,
	/// as this would not be helpful where they are actually used.
	/// </summary>
	struct WeightedPose
	{
		FTransform pose;
		float weight;
	};

	/// <summary>
	/// A pose (possibly) contributing to the global camera alignment pose.
	/// 
	/// A pose will only contribute if its fragmentId is the current fragmentId,
	/// and its distance weight based on its playSpacePosition is non-zero.
	/// If there are any ReferencePose's in the current fragment, at least one is guaranteed to have non-zero
	/// contribution, but it is possible that none are in the current fragment.
	/// </summary>
	class FReferencePose
	{
	public:
		FTransform virtualPose;

		/// <summary>
		///  The world locked space pose.
		/// </summary>
		FTransform LockedPose()
		{
			return lockedPose;
		}

		void SetLockedPose(FTransform input)
		{
			lockedPose = input;
			CheckAttachmentPoint();
			AfterAdjustmentPoseChanged();
		}

		/// <summary>
		/// Whether this reference pose should contribute now.
		/// </summary>
		bool IsActive(FrozenWorld_FragmentId CurrentFragmentId)
		{
			return fragmentId == CurrentFragmentId;
		}

	public:
		FString name;
		FrozenWorld_FragmentId fragmentId;
		FrozenWorld_AnchorId anchorId;

	private:
		FTransform lockedPose;

		TSharedPtr<FAttachmentPoint> AttachmentPoint = nullptr;
		FAttachmentPoint::FAdjustLocationDelegate LocationHandler;

	private:
		void CheckAttachmentPoint();

		void OnLocationUpdate(FTransform adjustment);

		void AfterAdjustmentPoseChanged()
		{
			/// Do any adjustment pose dependent caching here.
		}
	};

	/// <summary>
	/// Persistent database for reference poses.
	/// </summary>
	class FReferencePoseDB
	{
		/// <summary>
		/// A data element containing minimal information to reconstruct its corresponding reference point.
		/// </summary>
		struct Element
		{
		public:
			// The virtual (modeling space) pose.
			FTransform virtualPose;
			// The world locked space pose.
			FTransform lockedPose;

		public:
			void Write(IFileHandle* FileHandle);

			static Element Read(IFileHandle* FileHandle);
		};

	public:
		/// <summary>
		/// Add or update a reference pose to the database.
		/// </summary>
		/// <param name="refPose">The reference pose to add/update to the database.</param>
		/// <returns>True on success.</returns>
		bool Set(FReferencePose refPose)
		{
			Element elem{ refPose.virtualPose, refPose.LockedPose() };
			if (!data.Contains(refPose.name))
			{
				data.Add(refPose.name, elem);
			}
			else
			{
				data[refPose.name] = elem;
			}

			return true;
		}

		bool Get(FString uniqueName, FReferencePose& outRefPose);

		/// <summary>
		/// Delete an element from the database.
		/// </summary>
		/// <param name="uniqueName">The name of the element to delete.</param>
		/// <returns>True if the element was in the database prior to deletion.</returns>
		void Forget(FString uniqueName)
		{
			data.Remove(uniqueName);
		}

		/// <summary>
		/// Clear the database.
		/// </summary>
		void Empty()
		{
			data.Empty();
		}

		bool Save();
		bool Load();

	private:
		// current database version.
		uint32 version = 1;
		TMap<FString, Element> data;

		bool IsLoaded = false;
	};

	/// <summary>
	/// Unreal level implementation of aligning Unreal's coordinate system  
	/// with a discrete finite set of markers in the real world.
	/// 
	/// In addition to anchoring the otherwise arbitrary WorldLocked coordinate space to
	/// this set of correspondences, this addresses the tracker-scale issue, whereby due to
	/// tracker error, traversing a known distance in the real world traverses a different distance
	/// in Unreal space. This means that, given a large object of length L meters in Unreal space,
	/// starting at one end and walking L meters will not end up at the other end of the object,
	/// but only within +- 10% of L.
	/// Use of this service gives fairly exact correspondence at alignment points, and by interpolation
	/// gives fairly accurate correspondence within the convex set of alignment points.
	/// Note that no extrapolation is done, so outside the convex set of alignment points results,
	/// particularly with respect to scale compensation, will be less accurate.
	/// </summary>
	class FAlignmentManager : public IModularFeature
	{
	public:
		static FAlignmentManager* Get();
		FAlignmentManager();
		~FAlignmentManager();

		static FSimpleMulticastDelegate OnAlignmentManagerLoad;
		static FSimpleMulticastDelegate OnAlignmentManagerReset;

	private:
		static const FName GetModularFeatureName()
		{
			return FName(FName(TEXT("WLT_AlignmentManager")));
		}

	public:
		void ComputePinnedPose(FTransform lockedHeadPose);

	public:
		FTransform PinnedFromLocked;

	private:
		void CheckSave();
		void CheckSend();
		void CheckFragment();

		TArray<WeightedPose> ComputePoseWeights(FVector lockedHeadPosition);
		FTransform WeightedAverage(TArray<WeightedPose> poses);
		WeightedPose WeightedAverage(WeightedPose lhs, WeightedPose rhs);
		FTransform ComputePinnedFromLocked(FReferencePose refPose);
		void PerformSendAlignmentAnchors();
		void ActivateCurrentFragment();
		void BuildTriangulation();
		void InitTriangulator();

	public:
		void ClearAlignmentAnchors();
		void SendAlignmentAnchors();
		FrozenWorld_AnchorId AddAlignmentAnchor(FString uniqueName, FTransform virtualPose, FTransform lockedPose);
		bool GetAlignmentPose(FrozenWorld_AnchorId AnchorID, FTransform& outLockedPose);
		bool RemoveAlignmentAnchor(FrozenWorld_AnchorId AnchorID);

		bool Save();
		bool Load();
		FrozenWorld_AnchorId RestoreAlignmentAnchor(FString uniqueName, FTransform virtualPose);
		
		FrozenWorld_AnchorId ClaimAnchorId()
		{
			return nextAnchorId++;
		}

	private:
		TArray<FReferencePose> referencePoses;
		TArray<FReferencePose> sentPoses;
		TArray<FReferencePose> activePoses;
		TArray<WeightedPose> weightedPoses;
		TArray<FReferencePose> referencePosesToSave;

		FrozenWorld_FragmentId activeFragmentId = FrozenWorld_FragmentId_UNKNOWN;
		FrozenWorld_AnchorId nextAnchorId = FrozenWorld_AnchorId_INVALID + 1;

		bool needSave = false;
		bool needSend = false;
		bool needFragment = false;
		FReferencePoseDB poseDB;

		FTriangulator triangulator;

		void QueueForSave(FReferencePose refPose);
	};
}