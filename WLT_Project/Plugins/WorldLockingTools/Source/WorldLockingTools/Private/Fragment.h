// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#pragma warning(push)
#pragma warning(disable: 4996)
#include "FrozenWorldEngine.h"
#pragma warning(pop)

#include "AttachmentPoint.h"

namespace WorldLockingTools
{
	/// <summary>
	/// Fragment class is a container for attachment points in the same WorldLocking Fragment.
	/// It manages their update and adjustment, including merging in the attachment points from
	/// another fragment.
	/// </summary>
	class FFragment
	{
	public:
		FFragment(FrozenWorld_FragmentId fragmentId);

		void UpdateState(AttachmentPointStateType attachmentState);
		void AddAttachmentPoint(TSharedPtr<FAttachmentPoint> attachPoint);
		void ReleaseAttachmentPoint(TSharedPtr<FAttachmentPoint> attachmentPoint);

		void ReleaseAll();

		void AbsorbOtherFragment(FFragment other);
		void AbsorbOtherFragment(FFragment other, FTransform adjustment);

		void AdjustAll();

	public:
		FrozenWorld_FragmentId FragmentId;
		AttachmentPointStateType State;

	private:
		TArray<TSharedPtr<FAttachmentPoint>> attachmentList;

		TArray<FAttachmentPoint::FAdjustStateDelegate> updateStateAllAttachments;
	};
}