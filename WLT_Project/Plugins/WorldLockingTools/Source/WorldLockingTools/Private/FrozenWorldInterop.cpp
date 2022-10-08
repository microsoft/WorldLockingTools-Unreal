// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "FrozenWorldInterop.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "WorldLockingToolsModule.h"

namespace WorldLockingTools
{
	void FFrozenWorldInterop::checkError()
	{
		if (FW_GetError())
		{
			int bufsize = 256;
			char* buffer = new char[bufsize];
			int msgsize = FW_GetErrorMessage(bufsize, buffer);

			FString msg = UTF8_TO_TCHAR(buffer);
			UE_LOG(LogWLT, Error, TEXT("%s"), *msg);
		}
	}

	void FFrozenWorldInterop::ClearFrozenAnchors()
	{
		if (FW_GetNumAnchors(FrozenWorld_Snapshot::FrozenWorld_Snapshot_FROZEN) > 0)
		{
			FW_ClearAnchors(FrozenWorld_Snapshot::FrozenWorld_Snapshot_FROZEN);
			checkError();
		}
	}

	void FFrozenWorldInterop::ClearSpongyAnchors()
	{
		if (FW_GetNumAnchors(FrozenWorld_Snapshot::FrozenWorld_Snapshot_SPONGY) > 0)
		{
			FW_ClearAnchors(FrozenWorld_Snapshot::FrozenWorld_Snapshot_SPONGY);
			checkError();
		}
	}

	void FFrozenWorldInterop::Step_Init(FTransform spongyHeadPose)
	{
		FW_Step_Init();
		checkError();

		auto pos = UtoF(spongyHeadPose.GetLocation());
		auto fwdir = UtoF(spongyHeadPose.GetRotation().GetForwardVector(), 1);
		auto updir = UtoF(spongyHeadPose.GetRotation().GetUpVector(), 1);
		FW_SetHead(FrozenWorld_Snapshot_SPONGY, &pos, &fwdir, &updir);
		checkError();
	}

	void FFrozenWorldInterop::AddSpongyAnchors(TArray<FrozenWorld_Anchor> anchors)
	{
		if (anchors.Num() == 0)
		{
			return;
		}

		FW_AddAnchors(FrozenWorld_Snapshot_SPONGY, anchors.Num(), &anchors[0]);
		checkError();
	}

	void FFrozenWorldInterop::SetMostSignificantSpongyAnchorId(FrozenWorld_AnchorId anchorId)
	{
		FW_SetMostSignificantAnchorId(FrozenWorld_Snapshot_SPONGY, anchorId);
		checkError();
	}

	void FFrozenWorldInterop::AddSpongyEdges(TArray<FrozenWorld_Edge> edges)
	{
		if (edges.Num() == 0)
		{
			return;
		}

		FW_AddEdges(FrozenWorld_Snapshot_SPONGY, edges.Num(), &edges[0]);
		checkError();
	}

	void FFrozenWorldInterop::Step_Finish()
	{
		FW_Step_GatherSupports();
		checkError();

		FW_Step_AlignSupports();
		checkError();

		FW_GetMetrics(&metrics);
		checkError();
	}

	FrozenWorld_Metrics FFrozenWorldInterop::GetMetrics()
	{
		return metrics;
	}

	void FFrozenWorldInterop::RemoveFrozenAnchor(FrozenWorld_AnchorId anchorId)
	{
		FW_RemoveAnchor(FrozenWorld_Snapshot_FROZEN, anchorId);
		checkError();
	}

	FrozenWorld_FragmentId FFrozenWorldInterop::GetMostSignificantFragmentId()
	{
		FrozenWorld_FragmentId res;
		FW_GetMostSignificantFragmentId(FrozenWorld_Snapshot_FROZEN, &res);
		checkError();

		return res;
	}

	void FFrozenWorldInterop::CreateAttachmentPointFromHead(FVector frozenPosition, FrozenWorld_AnchorId& outAnchorId, FVector outLocationFromAnchor)
	{
		FrozenWorld_AttachmentPoint att;
		FrozenWorld_Vector v = UtoF(frozenPosition);
		FW_Tracking_CreateFromHead(&v, &att);
		checkError();
		outAnchorId = att.anchorId;
		outLocationFromAnchor = FtoU(att.locationFromAnchor);
	}

	void FFrozenWorldInterop::CreateAttachmentPointFromSpawner(FrozenWorld_AnchorId contextAnchorId, FVector contextLocationFromAnchor, FVector frozenPosition,
		FrozenWorld_AnchorId& outAnchorId, FVector& outLocationFromAnchor)
	{
		FrozenWorld_AttachmentPoint context;
		context.anchorId = contextAnchorId;
		context.locationFromAnchor = UtoF(contextLocationFromAnchor);
		
		FrozenWorld_AttachmentPoint att;
		FrozenWorld_Vector v = UtoF(frozenPosition);
		FW_Tracking_CreateFromSpawner(&context, &v, &att);
		checkError();
		outAnchorId = att.anchorId;
		outLocationFromAnchor = FtoU(att.locationFromAnchor);
	}

	bool FFrozenWorldInterop::ComputeAttachmentPointAdjustment(FrozenWorld_AnchorId oldAnchorId, FVector oldLocationFromAnchor,
		FrozenWorld_AnchorId& outNewAnchorId, FVector& outNewLocationFromAnchor, FTransform& outAdjustment)
	{
		FrozenWorld_AttachmentPoint attachmentPoint;
		
		attachmentPoint.anchorId = oldAnchorId;
		attachmentPoint.locationFromAnchor = UtoF(oldLocationFromAnchor);

		FrozenWorld_Transform fwAdjustment;
		bool adjusted = FW_RefitRefreeze_CalcAdjustment(&attachmentPoint, &fwAdjustment);
		checkError();
		outNewAnchorId = attachmentPoint.anchorId;
		outNewLocationFromAnchor = FtoU(attachmentPoint.locationFromAnchor);
		outAdjustment = FtoU(fwAdjustment);

		return adjusted;
	}

	bool FFrozenWorldInterop::Merge(FrozenWorld_FragmentId& outTargetFragment, TArray<FragmentPose> outMergedFragments)
	{
		outTargetFragment = FrozenWorld_FragmentId_INVALID;

		if (!FW_RefitMerge_Init())
		{
			checkError();
			outTargetFragment = GetMostSignificantFragmentId();
			return false;
		}
		checkError();

		FW_RefitMerge_Prepare();
		checkError();

		int bufSize = FW_RefitMerge_GetNumAdjustedFragments();
		checkError();

		FrozenWorld_RefitMerge_AdjustedFragment* buf = new FrozenWorld_RefitMerge_AdjustedFragment[bufSize];
		int numAdjustedFragments = FW_RefitMerge_GetAdjustedFragments(bufSize, buf);
		checkError();

		for (int i = 0; i < numAdjustedFragments; i++)
		{
			auto fragmentAdjust = FragmentPose{ buf[i].fragmentId, FtoU(buf[i].adjustment) };
			outMergedFragments.Add(fragmentAdjust);
		}

		FW_RefitMerge_GetMergedFragmentId(&outTargetFragment);
		checkError();

		FW_RefitMerge_Apply();
		checkError();

		return true;
	}

	bool FFrozenWorldInterop::Refreeze(FrozenWorld_FragmentId& outMergedId, TArray<FrozenWorld_FragmentId> outAbsorbedFragments)
	{
		if (!FW_RefitRefreeze_Init())
		{
			checkError();
			outMergedId = GetMostSignificantFragmentId();
			return false;
		}
		checkError();

		FW_RefitRefreeze_Prepare();
		checkError();

		int bufSize = FW_RefitRefreeze_GetNumAdjustedFragments();
		checkError();

		FrozenWorld_FragmentId* buf = new FrozenWorld_FragmentId[bufSize];

		int numAffected = FW_RefitRefreeze_GetAdjustedFragmentIds(bufSize, buf);
		checkError();

		for (int i = 0; i < numAffected; ++i)
		{
			outAbsorbedFragments.Add(buf[i]);
		}

		FW_RefitRefreeze_GetMergedFragmentId(&outMergedId);
		checkError();

		return true;
	}

	void FFrozenWorldInterop::RefreezeFinish()
	{
		FW_RefitRefreeze_Apply();
		checkError();
	}

	FTransform FFrozenWorldInterop::GetAlignment()
	{
		FrozenWorld_Transform spongyFromFrozenTrans;
		FW_GetAlignment(&spongyFromFrozenTrans);
		checkError();
		return FtoU(spongyFromFrozenTrans);
	}

	FTransform FFrozenWorldInterop::GetSpongyHead()
	{
		FrozenWorld_Vector pos;
		FrozenWorld_Vector fwdir;
		FrozenWorld_Vector updir;
		FW_GetHead(FrozenWorld_Snapshot_SPONGY, &pos, &fwdir, &updir);
		checkError();

		FQuat rot = FRotationMatrix::MakeFromXZ(FtoU(fwdir, 1), FtoU(updir, 1)).ToQuat();

		return FTransform(rot, FtoU(pos));
		
	}

	void FFrozenWorldInterop::Dispose()
	{
		FW_Destroy();
		checkError();
	}

	void FFrozenWorldInterop::ResetAlignment(FTransform pose)
	{
		auto alignment = UtoF(pose);
		FW_SetAlignment(&alignment);
		checkError();
	}

	void FFrozenWorldInterop::SerializeOpen(FrozenWorld_Serialize_Stream* streamInOut)
	{
		FW_Serialize_Open(streamInOut);
		checkError();
	}

	void FFrozenWorldInterop::SerializeGather(FrozenWorld_Serialize_Stream* streamInOut)
	{
		FW_Serialize_Gather(streamInOut);
		checkError();
	}

	int FFrozenWorldInterop::SerializeRead(FrozenWorld_Serialize_Stream* streamInOut, int bytesBufferSize, char* bytesOut)
	{
		int numBytesRead;
		numBytesRead = FW_Serialize_Read(streamInOut, bytesBufferSize, bytesOut);
		checkError();

		return numBytesRead;
	}

	void FFrozenWorldInterop::SerializeClose(FrozenWorld_Serialize_Stream* streamInOut)
	{
		FW_Serialize_Close(streamInOut);
		checkError();
	}

	void FFrozenWorldInterop::DeserializeOpen(FrozenWorld_Deserialize_Stream* streamInOut)
	{
		FW_Deserialize_Open(streamInOut);
		checkError();
	}

	int FFrozenWorldInterop::DeserializeWrite(FrozenWorld_Deserialize_Stream* streamInOut, int numBytes, char* bytes)
	{
		int numBytesWritten = FW_Deserialize_Write(streamInOut, numBytes, bytes);
		checkError();

		return numBytesWritten;
	}

	void FFrozenWorldInterop::DeserializeApply(FrozenWorld_Deserialize_Stream* streamInOut)
	{
		FW_Deserialize_Apply(streamInOut);
		checkError();
	}

	void FFrozenWorldInterop::DeserializeClose(FrozenWorld_Deserialize_Stream* streamInOut)
	{
		FW_Deserialize_Close(streamInOut);
		checkError();
	}

	TArray<FrozenWorld_AnchorId> FFrozenWorldInterop::GetFrozenAnchorIds()
	{
		int numAnchors = FW_GetNumAnchors(FrozenWorld_Snapshot_FROZEN);
		checkError();

		TArray<FrozenWorld_Anchor> fwa;
		fwa.AddUninitialized(numAnchors);

		TArray<FrozenWorld_AnchorId> res;

		if (numAnchors > 0)
		{
			FW_GetAnchors(FrozenWorld_Snapshot_FROZEN, numAnchors, &fwa[0]);
			checkError();

			for (int i = 0; i < numAnchors; i++)
			{
				res.Add(fwa[i].anchorId);
			}
		}

		return res;
	}

	void FFrozenWorldInterop::LoadFrozenWorld()
	{
#if defined(USING_FROZEN_WORLD)
		const FString PluginBaseDir = IPluginManager::Get().FindPlugin("WorldLockingTools")->GetBaseDir();
		FString PackageRelativePath = PluginBaseDir / THIRDPARTY_BINARY_SUBFOLDER;

		// On HoloLens, DLLs must be loaded relative to the package with no ".."'s in the path. 
		// If using FPlatformProcess::PushDLLDirectory, the library path must be made relative to the RootDir.
#if PLATFORM_HOLOLENS
		FPaths::MakePathRelativeTo(PackageRelativePath, *(FPaths::RootDir() + TEXT("/")));
#endif

		const FString FrozenWorldDLL = "FrozenWorldPlugin.dll";

		FPlatformProcess::PushDllDirectory(*PackageRelativePath);
		if ((FrozenWorldHandle = FPlatformProcess::GetDllHandle(*FrozenWorldDLL)) == nullptr)
		{
			UE_LOG(LogWLT, Warning, TEXT("Dll \'%s\' can't be loaded from \'%s\'"), *FrozenWorldDLL, *PackageRelativePath);
		}
		FPlatformProcess::PopDllDirectory(*PackageRelativePath);

		// Version
		FW_GetVersion = (FW_GetVersionPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetVersion")));
		check(FW_GetVersion != nullptr);

		// Errors and diagnostics
		FW_GetError = (FW_GetErrorPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetError")));
		check(FW_GetError != nullptr);
		FW_GetErrorMessage = (FW_GetErrorMessagePtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetErrorMessage")));
		check(FW_GetErrorMessage != nullptr);

		// Startup and teardown
		FW_Init = (FW_InitPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Init")));
		check(FW_Init != nullptr);
		FW_Destroy = (FW_DestroyPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Destroy")));
		check(FW_Destroy != nullptr);

		// Alignment
		FW_Step_Init = (FW_Step_InitPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Step_Init")));
		check(FW_Step_Init != nullptr);
		FW_Step_GatherSupports = (FW_Step_GatherSupportsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Step_GatherSupports")));
		check(FW_Step_GatherSupports != nullptr);
		FW_Step_AlignSupports = (FW_Step_AlignSupportsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Step_AlignSupports")));
		check(FW_Step_AlignSupports != nullptr);

		// Alignment configuration
		FW_GetAlignConfig = (FW_GetAlignConfigPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetAlignConfig")));
		check(FW_GetAlignConfig != nullptr);
		FW_SetAlignConfig = (FW_SetAlignConfigPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_SetAlignConfig")));
		check(FW_SetAlignConfig != nullptr);

		// Supports access
		FW_GetNumSupports = (FW_GetNumSupportsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetNumSupports")));
		check(FW_GetNumSupports != nullptr);
		FW_GetSupports = (FW_GetSupportsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetSupports")));
		check(FW_GetSupports != nullptr);
		FW_SetSupports = (FW_SetSupportsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_SetSupports")));
		check(FW_SetSupports != nullptr);

		// Snapshot access: Head and alignment
		FW_GetHead = (FW_GetHeadPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetHead")));
		check(FW_GetHead != nullptr);
		FW_SetHead = (FW_SetHeadPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_SetHead")));
		check(FW_SetHead != nullptr);
		FW_GetAlignment = (FW_GetAlignmentPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetAlignment")));
		check(FW_GetAlignment != nullptr);
		FW_SetAlignment = (FW_SetAlignmentPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_SetAlignment")));
		check(FW_SetAlignment != nullptr);

		// Snapshot access: Most significant anchor
		FW_GetMostSignificantAnchorId = (FW_GetMostSignificantAnchorIdPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetMostSignificantAnchorId")));
		check(FW_GetMostSignificantAnchorId != nullptr);
		FW_SetMostSignificantAnchorId = (FW_SetMostSignificantAnchorIdPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_SetMostSignificantAnchorId")));
		check(FW_SetMostSignificantAnchorId != nullptr);
		FW_GetMostSignificantFragmentId = (FW_GetMostSignificantFragmentIdPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetMostSignificantFragmentId")));
		check(FW_GetMostSignificantFragmentId != nullptr);

		// Snapshot access: Anchors
		FW_GetNumAnchors = (FW_GetNumAnchorsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetNumAnchors")));
		check(FW_GetNumAnchors != nullptr);
		FW_GetAnchors = (FW_GetAnchorsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetAnchors")));
		check(FW_GetAnchors != nullptr);
		FW_AddAnchors = (FW_AddAnchorsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_AddAnchors")));
		check(FW_AddAnchors != nullptr);
		FW_SetAnchorTransform = (FW_SetAnchorTransformPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_SetAnchorTransform")));
		check(FW_SetAnchorTransform != nullptr);
		FW_SetAnchorFragment = (FW_SetAnchorFragmentPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_SetAnchorFragment")));
		check(FW_SetAnchorFragment != nullptr);
		FW_RemoveAnchor = (FW_RemoveAnchorPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RemoveAnchor")));
		check(FW_RemoveAnchor != nullptr);
		FW_ClearAnchors = (FW_ClearAnchorsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_ClearAnchors")));
		check(FW_ClearAnchors != nullptr);

		// Snapshot access: Edges
		FW_GetNumEdges = (FW_GetNumEdgesPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetNumEdges")));
		check(FW_GetNumEdges != nullptr);
		FW_GetEdges = (FW_GetEdgesPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetEdges")));
		check(FW_GetEdges != nullptr);
		FW_AddEdges = (FW_AddEdgesPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_AddEdges")));
		check(FW_AddEdges != nullptr);
		FW_RemoveEdge = (FW_RemoveEdgePtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RemoveEdge")));
		check(FW_RemoveEdge != nullptr);
		FW_ClearEdges = (FW_ClearEdgesPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_ClearEdges")));
		check(FW_ClearEdges != nullptr);

		// Snapshot access: Utilities
		FW_MergeAnchorsAndEdges = (FW_MergeAnchorsAndEdgesPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_MergeAnchorsAndEdges")));
		check(FW_MergeAnchorsAndEdges != nullptr);
		FW_GuessMissingEdges = (FW_GuessMissingEdgesPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GuessMissingEdges")));
		check(FW_GuessMissingEdges != nullptr);

		// Metrics
		FW_GetMetrics = (FW_GetMetricsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetMetrics")));
		check(FW_GetMetrics != nullptr);

		// Metrics configuration
		FW_GetMetricsConfig = (FW_GetMetricsConfigPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_GetMetricsConfig")));
		check(FW_GetMetricsConfig != nullptr);
		FW_SetMetricsConfig = (FW_SetMetricsConfigPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_SetMetricsConfig")));
		check(FW_SetMetricsConfig != nullptr);

		// Scene object tracking
		FW_Tracking_CreateFromHead = (FW_Tracking_CreateFromHeadPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Tracking_CreateFromHead")));
		check(FW_Tracking_CreateFromHead != nullptr);
		FW_Tracking_CreateFromSpawner = (FW_Tracking_CreateFromSpawnerPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Tracking_CreateFromSpawner")));
		check(FW_Tracking_CreateFromSpawner != nullptr);
		FW_Tracking_Move = (FW_Tracking_MovePtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Tracking_Move")));
		check(FW_Tracking_Move != nullptr);

		// Fragment merge
		FW_RefitMerge_Init = (FW_RefitMerge_InitPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitMerge_Init")));
		check(FW_RefitMerge_Init != nullptr);
		FW_RefitMerge_Prepare = (FW_RefitMerge_PreparePtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitMerge_Prepare")));
		check(FW_RefitMerge_Prepare != nullptr);
		FW_RefitMerge_Apply = (FW_RefitMerge_ApplyPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitMerge_Apply")));
		check(FW_RefitMerge_Apply != nullptr);

		// Fragment merge: Adjustments query
		FW_RefitMerge_GetNumAdjustedFragments = (FW_RefitMerge_GetNumAdjustedFragmentsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitMerge_GetNumAdjustedFragments")));
		check(FW_RefitMerge_GetNumAdjustedFragments != nullptr);
		FW_RefitMerge_GetAdjustedFragments = (FW_RefitMerge_GetAdjustedFragmentsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitMerge_GetAdjustedFragments")));
		check(FW_RefitMerge_GetAdjustedFragments != nullptr);
		FW_RefitMerge_GetAdjustedAnchorIds = (FW_RefitMerge_GetAdjustedAnchorIdsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitMerge_GetAdjustedAnchorIds")));
		check(FW_RefitMerge_GetAdjustedAnchorIds != nullptr);
		FW_RefitMerge_GetMergedFragmentId = (FW_RefitMerge_GetMergedFragmentIdPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitMerge_GetMergedFragmentId")));
		check(FW_RefitMerge_GetMergedFragmentId != nullptr);

		// Refreeze
		FW_RefitRefreeze_Init = (FW_RefitRefreeze_InitPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitRefreeze_Init")));
		check(FW_RefitRefreeze_Init != nullptr);
		FW_RefitRefreeze_Prepare = (FW_RefitRefreeze_PreparePtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitRefreeze_Prepare")));
		check(FW_RefitRefreeze_Prepare != nullptr);
		FW_RefitRefreeze_Apply = (FW_RefitRefreeze_ApplyPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitRefreeze_Apply")));
		check(FW_RefitRefreeze_Apply != nullptr);

		// Refreeze: Adjustments query
		FW_RefitRefreeze_GetNumAdjustedFragments = (FW_RefitRefreeze_GetNumAdjustedFragmentsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitRefreeze_GetNumAdjustedFragments")));
		check(FW_RefitRefreeze_GetNumAdjustedFragments != nullptr);
		FW_RefitRefreeze_GetNumAdjustedAnchors = (FW_RefitRefreeze_GetNumAdjustedAnchorsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitRefreeze_GetNumAdjustedAnchors")));
		check(FW_RefitRefreeze_GetNumAdjustedAnchors != nullptr);
		FW_RefitRefreeze_GetAdjustedFragmentIds = (FW_RefitRefreeze_GetAdjustedFragmentIdsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitRefreeze_GetAdjustedFragmentIds")));
		check(FW_RefitRefreeze_GetAdjustedFragmentIds != nullptr);
		FW_RefitRefreeze_GetAdjustedAnchorIds = (FW_RefitRefreeze_GetAdjustedAnchorIdsPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitRefreeze_GetAdjustedAnchorIds")));
		check(FW_RefitRefreeze_GetAdjustedAnchorIds != nullptr);
		FW_RefitRefreeze_CalcAdjustment = (FW_RefitRefreeze_CalcAdjustmentPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitRefreeze_CalcAdjustment")));
		check(FW_RefitRefreeze_CalcAdjustment != nullptr);
		FW_RefitRefreeze_GetMergedFragmentId = (FW_RefitRefreeze_GetMergedFragmentIdPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_RefitRefreeze_GetMergedFragmentId")));
		check(FW_RefitRefreeze_GetMergedFragmentId != nullptr);

		// Persistence: Serialization
		FW_Serialize_Open = (FW_Serialize_OpenPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Serialize_Open")));
		check(FW_Serialize_Open != nullptr);
		FW_Serialize_Gather = (FW_Serialize_GatherPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Serialize_Gather")));
		check(FW_Serialize_Gather != nullptr);
		FW_Serialize_Read = (FW_Serialize_ReadPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Serialize_Read")));
		check(FW_Serialize_Read != nullptr);
		FW_Serialize_Close = (FW_Serialize_ClosePtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Serialize_Close")));
		check(FW_Serialize_Close != nullptr);

		// Persistence: Deserialization
		FW_Deserialize_Open = (FW_Deserialize_OpenPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Deserialize_Open")));
		check(FW_Deserialize_Open != nullptr);
		FW_Deserialize_Write = (FW_Deserialize_WritePtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Deserialize_Write")));
		check(FW_Deserialize_Write != nullptr);
		FW_Deserialize_Apply = (FW_Deserialize_ApplyPtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Deserialize_Apply")));
		check(FW_Deserialize_Apply != nullptr);
		FW_Deserialize_Close = (FW_Deserialize_ClosePtr)(FPlatformProcess::GetDllExport(FrozenWorldHandle, TEXT("FrozenWorld_Deserialize_Close")));
		check(FW_Deserialize_Close != nullptr);
#endif
	}
}