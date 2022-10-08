// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "FrozenWorldInterop.h"
#include "AnchorManager.h"
#include "FragmentManager.h"
#include "AlignmentManager.h"

#include "Components/SceneComponent.h"

#include "Features/IModularFeatures.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

#include "WorldLockingToolsTypes.h"

namespace WorldLockingTools
{
	// Helper class for starting a background thread.
	class BackgroundOperation : public FRunnable 
	{
	private:
		TFunction<void()> BackgroundTask;

	public:
		void QueueBackgroundTask(TFunction<void()> RunTask)
		{
			BackgroundTask = RunTask;
			FRunnableThread::Create(this, TEXT(""), 0, TPri_BelowNormal);
		}

		uint32 Run() override 
		{
			BackgroundTask();
			return 0;
		}
	};

	class FFrozenWorldPlugin : 
		public IModularFeature,
		public TSharedFromThis<FFrozenWorldPlugin, ESPMode::ThreadSafe>
	{
	private:
		enum InitializationState
		{
			Uninitialized,
			Starting,
			Running
		};

		InitializationState initializationState = InitializationState::Uninitialized;

	public:
		static FFrozenWorldPlugin* Get();
		void Register();
		void Unregister();
		void HandleEndPIE(const bool InIsSimulating);
		void OnEndPlay(UWorld* InWorld);

		void Start(FWorldLockingToolsConfiguration Configuration);
		void Stop();
		void SaveAsync();
		void LoadAsync();
		void Reset();

		void Update();

	private:
		static const FName GetModularFeatureName()
		{
			return FName(FName(TEXT("WLT_FrozenWorldPlugin")));
		}

	private:
		FTransform pinnedFromLocked = FTransform::Identity;
		FTransform lockedFromPlayspace = FTransform::Identity;
		FTransform spongyFromCamera = FTransform::Identity;

	public:
		bool AutoLoad = true;
		bool AutoSave = true;
		float AutoSaveInterval = 10;
		bool AutoRefreeze = true;
		bool AutoMerge = true;
		bool Enabled = true;
		bool NoPitchAndRoll = false;

		FTransform FrozenFromSpongy();
		FTransform SpongyFromFrozen();
		FTransform PlayspaceFromSpongy();
		FTransform FrozenFromLocked();
		FTransform LockedFromFrozen();
		FTransform FrozenFromPinned();
		FTransform PinnedFromLocked();
		FTransform PinnedFromFrozen();
		FTransform LockedFromSpongy();

		TArray<FrozenWorld_AnchorId> GetFrozenAnchorIds();

	public:
		void ClearSpongyAnchors();
		void ClearFrozenAnchors();
		void Step_Init(FTransform spongyHeadPose);
		void AddSpongyAnchors(TArray<FrozenWorld_Anchor> anchors);
		void SetMostSignificantSpongyAnchorId(FrozenWorld_AnchorId anchorId);
		void AddSpongyEdges(TArray<FrozenWorld_Edge> edges);
		void Step_Finish();

		FrozenWorld_Metrics GetMetrics();

		void RemoveFrozenAnchor(FrozenWorld_AnchorId anchorId);

		FrozenWorld_FragmentId GetMostSignificantFragmentId();

		void CreateAttachmentPointFromHead(FVector frozenPosition, FrozenWorld_AnchorId& outAnchorId, FVector outLocationFromAnchor);
		void CreateAttachmentPointFromSpawner(FrozenWorld_AnchorId contextAnchorId, FVector contextLocationFromAnchor, FVector frozenPosition,
			FrozenWorld_AnchorId& outAnchorId, FVector& outLocationFromAnchor);

		bool ComputeAttachmentPointAdjustment(FrozenWorld_AnchorId oldAnchorId, FVector oldLocationFromAnchor,
			FrozenWorld_AnchorId& outNewAnchorId, FVector& outNewLocationFromAnchor, FTransform& outAdjustment);

		bool Merge(FrozenWorld_FragmentId& outTargetFragment, TArray<FragmentPose> outMergedFragments);
		bool Refreeze(FrozenWorld_FragmentId& outMergedId, TArray<FrozenWorld_FragmentId> outAbsorbedFragments);
		void RefreezeFinish();

	private:
		FFrozenWorldInterop FrozenWorldInterop;
		FAnchorManager FrozenWorldAnchorManager;
		FFragmentManager FrozenWorldFragmentManager;
		FAlignmentManager FrozenWorldAlignmentManager;

		USceneComponent* CameraParent = nullptr;
		USceneComponent* AdjustmentFrame = nullptr;

		bool CacheCameraHierarchy();

	public:
		FFrozenWorldInterop GetFrozenWorldInterop()
		{
			return FrozenWorldInterop;
		}

	private:
		bool HasPendingIO()
		{
			return hasPendingSaveTask || hasPendingLoadTask;
		}

		bool hasPendingSaveTask = false;
		bool hasPendingLoadTask = false;

		float LastSavingTime;

		FString frozenWorldFile;
		FString stateFileNameBase;
	};
}	 // namespace WorldLockingTools