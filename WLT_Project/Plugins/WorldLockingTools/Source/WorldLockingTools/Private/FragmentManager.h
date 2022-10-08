// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#pragma warning(push)
#pragma warning(disable: 4996)
#include "FrozenWorldEngine.h"
#pragma warning(pop)

#include "Fragment.h"
#include "AttachmentPoint.h"
#include "FrozenWorldInterop.h"

#include "Features/IModularFeatures.h"

namespace WorldLockingTools
{
	struct PendingAttachmentPoint
	{
		TSharedPtr<FAttachmentPoint> target;
		TSharedPtr<FAttachmentPoint> context;
	};

	/// <summary>
	/// Manager for multiple fragments (isolated islands of spatial relevance).
	/// </summary>
	class FFragmentManager : public IModularFeature
	{
	public:
		static FFragmentManager* Get();

		FFragmentManager();
		~FFragmentManager();

		void Pause();
		void Update(bool autoRefreeze, bool autoMerge);

		void Reset();

		void ApplyActiveCurrentFragment();
		void SetupAttachmentPoint(TSharedPtr<FAttachmentPoint> target, TSharedPtr<FAttachmentPoint> context);
		void AddPendingAttachmentPoint(TSharedPtr<FAttachmentPoint> attachPoint, TSharedPtr<FAttachmentPoint> context);

		TSharedPtr<FAttachmentPoint> CreateAttachmentPoint(
			FVector frozenPosition,
			TSharedPtr<FAttachmentPoint> context,
			FAttachmentPoint::FAdjustLocationDelegate LocationHandler,
			FAttachmentPoint::FAdjustStateDelegate StateHandler);
		void TeleportAttachmentPoint(TSharedPtr<FAttachmentPoint> attachPointIface, FVector newFrozenPosition, TSharedPtr<FAttachmentPoint> context);
		void ReleaseAttachmentPoint(TSharedPtr<FAttachmentPoint> AttachmentPoint);

		bool Merge();
		bool Refreeze();

		DECLARE_DELEGATE_TwoParams(FRefitNotificationDelegate, FrozenWorld_FragmentId, TArray<FrozenWorld_FragmentId>);
		FRefitNotificationDelegate refitNotifications;

	private:
		static const FName GetModularFeatureName()
		{
			return FName(FName(TEXT("WLT_FragmentManager")));
		}

	private:
		TSharedPtr<FFragment> EnsureFragment(FrozenWorld_FragmentId id);
		void ProcessPendingAttachmentPoints();

		TArray<FrozenWorld_FragmentId> ExtractFragmentIds(TArray<FragmentPose> source);

		FrozenWorld_FragmentId CurrentFragmentId;

	public:
		FrozenWorld_FragmentId GetCurrentFragmentId()
		{
			return CurrentFragmentId;
		}

	private:
		TMap<FrozenWorld_FragmentId, TSharedPtr<FFragment>> fragments;
		TArray<PendingAttachmentPoint> pendingAttachments;

		FrozenWorld_FragmentId GetTargetFragmentId(TSharedPtr<FAttachmentPoint> context);
		void ChangeAttachmentPointFragment(FrozenWorld_FragmentId oldFragmentId, TSharedPtr<FAttachmentPoint> attachPoint);
	};
}