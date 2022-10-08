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
	struct FragmentPose
	{
		FrozenWorld_FragmentId fragmentId;
		FTransform pose;
	};

	class FFrozenWorldInterop
	{
	public:
		void LoadFrozenWorld();

		FrozenWorld_Metrics metrics;

	private:
		void* FrozenWorldHandle;

	public:
		static FrozenWorld_Vector UtoF(FVector3d v, float scale = 100.0f)
		{
			return FrozenWorld_Vector{ (float)(v.Y) / scale, (float)(v.Z) / scale, (float)(v.X) / scale };
		}
		static FrozenWorld_Quaternion UtoF(FQuat4d q)
		{
			return FrozenWorld_Quaternion{ -(float)(q.Y), -(float)(q.Z), (float)(q.X), -(float)(q.W) };
		}
		static FrozenWorld_Transform UtoF(FTransform p)
		{
			return FrozenWorld_Transform{ UtoF(p.GetLocation()), UtoF(p.GetRotation()) };
		}


		static FVector FtoU(FrozenWorld_Vector v, float scale = 100.0f)
		{
			return FVector(v.z, v.x, v.y) * scale;
		}

		static FQuat FtoU(FrozenWorld_Quaternion q)
		{
			return FQuat(q.z, -q.x, -q.y, -q.w);
		}

		static FTransform FtoU(FrozenWorld_Transform p)
		{
			return FTransform(FtoU(p.rotation), FtoU(p.position));
		}

	private:
		void checkError();

	public:
		void ClearFrozenAnchors();
		void ClearSpongyAnchors();
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

		FTransform GetAlignment();
		FTransform GetSpongyHead();

		void Dispose();
		void ResetAlignment(FTransform pose);

		void SerializeOpen(FrozenWorld_Serialize_Stream* streamInOut);
		void SerializeGather(FrozenWorld_Serialize_Stream* streamInOut);
		int SerializeRead(FrozenWorld_Serialize_Stream* streamInOut, int bytesBufferSize, char* bytesOut);
		void SerializeClose(FrozenWorld_Serialize_Stream* streamInOut);

		void DeserializeOpen(FrozenWorld_Deserialize_Stream* streamInOut);
		int DeserializeWrite(FrozenWorld_Deserialize_Stream* streamInOut, int numBytes, char* bytes);
		void DeserializeApply(FrozenWorld_Deserialize_Stream* streamInOut);
		void DeserializeClose(FrozenWorld_Deserialize_Stream* streamInOut);

		TArray<FrozenWorld_AnchorId> GetFrozenAnchorIds();

	public:
		// Version
		typedef int(*FW_GetVersionPtr)(bool, int, char*);

		FW_GetVersionPtr FW_GetVersion;

		// Errors and diagnostics
		typedef bool(*FW_GetErrorPtr)();
		typedef int(*FW_GetErrorMessagePtr)(int messageBufferSize, char* messageOut);

		FW_GetErrorPtr FW_GetError;
		FW_GetErrorMessagePtr FW_GetErrorMessage;

		// Startup and teardown
		typedef void(*FW_InitPtr)();
		typedef void(*FW_DestroyPtr)();

		FW_InitPtr FW_Init;
		FW_DestroyPtr FW_Destroy;

		// Alignment
		typedef void(*FW_Step_InitPtr)();
		typedef int(*FW_Step_GatherSupportsPtr)();
		typedef void(*FW_Step_AlignSupportsPtr)();

		FW_Step_InitPtr FW_Step_Init;
		FW_Step_GatherSupportsPtr FW_Step_GatherSupports;
		FW_Step_AlignSupportsPtr FW_Step_AlignSupports;

		// Alignment configuration
		typedef void(*FW_GetAlignConfigPtr)(FrozenWorld_AlignConfig* configOut);
		typedef void(*FW_SetAlignConfigPtr)(FrozenWorld_AlignConfig* config);

		FW_GetAlignConfigPtr FW_GetAlignConfig;
		FW_SetAlignConfigPtr FW_SetAlignConfig;

		// Supports access
		typedef int(*FW_GetNumSupportsPtr)();
		typedef int(*FW_GetSupportsPtr)(int supportsBufferSize, FrozenWorld_Support* supportsOut);
		typedef void(*FW_SetSupportsPtr)(int numSupports, FrozenWorld_Support* supports);

		FW_GetNumSupportsPtr FW_GetNumSupports;
		FW_GetSupportsPtr FW_GetSupports;
		FW_SetSupportsPtr FW_SetSupports;

		// Snapshot access: Head and alignment
		typedef void(*FW_GetHeadPtr)(FrozenWorld_Snapshot snapshot, FrozenWorld_Vector* headPositionOut, FrozenWorld_Vector* headDirectionForwardOut, FrozenWorld_Vector* headDirectionUpOut);
		typedef void(*FW_SetHeadPtr)(FrozenWorld_Snapshot snapshot, FrozenWorld_Vector* headPosition, FrozenWorld_Vector* headDirectionForward, FrozenWorld_Vector* headDirectionUp);
		typedef void(*FW_GetAlignmentPtr)(FrozenWorld_Transform* spongyFromFrozenTransformOut);
		typedef void(*FW_SetAlignmentPtr)(FrozenWorld_Transform* spongyFromFrozenTransform);

		FW_GetHeadPtr FW_GetHead;
		FW_SetHeadPtr FW_SetHead;
		FW_GetAlignmentPtr FW_GetAlignment;
		FW_SetAlignmentPtr FW_SetAlignment;

		// Snapshot access: Most significant anchor
		typedef void(*FW_GetMostSignificantAnchorIdPtr)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId* anchorIdOut);
		typedef void(*FW_SetMostSignificantAnchorIdPtr)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId anchorId);
		typedef void(*FW_GetMostSignificantFragmentIdPtr)(FrozenWorld_Snapshot snapshot, FrozenWorld_FragmentId* fragmentIdOut);

		FW_GetMostSignificantAnchorIdPtr FW_GetMostSignificantAnchorId;
		FW_SetMostSignificantAnchorIdPtr FW_SetMostSignificantAnchorId;
		FW_GetMostSignificantFragmentIdPtr FW_GetMostSignificantFragmentId;

		// Snapshot access: Anchors
		typedef int(*FW_GetNumAnchorsPtr)(FrozenWorld_Snapshot snapshot);
		typedef int(*FW_GetAnchorsPtr)(FrozenWorld_Snapshot snapshot, int anchorsBufferSize, FrozenWorld_Anchor* anchorsOut);
		typedef void(*FW_AddAnchorsPtr)(FrozenWorld_Snapshot snapshot, int numAnchors, FrozenWorld_Anchor* anchors);
		typedef bool(*FW_SetAnchorTransformPtr)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId anchorId, FrozenWorld_Transform* transform);
		typedef bool(*FW_SetAnchorFragmentPtr)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId anchorId, FrozenWorld_FragmentId fragmentId);
		typedef bool(*FW_RemoveAnchorPtr)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId anchorId);
		typedef void(*FW_ClearAnchorsPtr)(FrozenWorld_Snapshot snapshot);

		FW_GetNumAnchorsPtr FW_GetNumAnchors;
		FW_GetAnchorsPtr FW_GetAnchors;
		FW_AddAnchorsPtr FW_AddAnchors;
		FW_SetAnchorTransformPtr FW_SetAnchorTransform;
		FW_SetAnchorFragmentPtr FW_SetAnchorFragment;
		FW_RemoveAnchorPtr FW_RemoveAnchor;
		FW_ClearAnchorsPtr FW_ClearAnchors;

		// Snapshot access: Edges
		typedef int(*FW_GetNumEdgesPtr)(FrozenWorld_Snapshot snapshot);
		typedef int(*FW_GetEdgesPtr)(FrozenWorld_Snapshot snapshot, int edgesBufferSize, FrozenWorld_Edge* edgesOut);
		typedef void(*FW_AddEdgesPtr)(FrozenWorld_Snapshot snapshot, int numEdges, FrozenWorld_Edge* edges);
		typedef bool(*FW_RemoveEdgePtr)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId anchorId1, FrozenWorld_AnchorId anchorId2);
		typedef void(*FW_ClearEdgesPtr)(FrozenWorld_Snapshot snapshot);

		FW_GetNumEdgesPtr FW_GetNumEdges;
		FW_GetEdgesPtr FW_GetEdges;
		FW_AddEdgesPtr FW_AddEdges;
		FW_RemoveEdgePtr FW_RemoveEdge;
		FW_ClearEdgesPtr FW_ClearEdges;

		// Snapshot access: Utilities
		typedef int(*FW_MergeAnchorsAndEdgesPtr)(FrozenWorld_Snapshot sourceSnapshot, FrozenWorld_Snapshot targetSnapshot);
		typedef int(*FW_GuessMissingEdgesPtr)(FrozenWorld_Snapshot snapshot, int guessedEdgesBufferSize, FrozenWorld_Edge* guessedEdgesOut);

		FW_MergeAnchorsAndEdgesPtr FW_MergeAnchorsAndEdges;
		FW_GuessMissingEdgesPtr FW_GuessMissingEdges;

		// Metrics
		typedef void(*FW_GetMetricsPtr)(FrozenWorld_Metrics* metricsOut);

		FW_GetMetricsPtr FW_GetMetrics;

		// Metrics configuration
		typedef void(*FW_GetMetricsConfigPtr)(FrozenWorld_MetricsConfig* configOut);
		typedef void(*FW_SetMetricsConfigPtr)(FrozenWorld_MetricsConfig* config);

		FW_GetMetricsConfigPtr FW_GetMetricsConfig;
		FW_SetMetricsConfigPtr FW_SetMetricsConfig;

		// Scene object tracking
		typedef void(*FW_Tracking_CreateFromHeadPtr)(FrozenWorld_Vector* frozenLocation, FrozenWorld_AttachmentPoint* attachmentPointOut);
		typedef void(*FW_Tracking_CreateFromSpawnerPtr)(FrozenWorld_AttachmentPoint* spawnerAttachmentPoint, FrozenWorld_Vector* frozenLocation, FrozenWorld_AttachmentPoint* attachmentPointOut);
		typedef void(*FW_Tracking_MovePtr)(FrozenWorld_Vector* targetFrozenLocation, FrozenWorld_AttachmentPoint* attachmentPointInOut);

		FW_Tracking_CreateFromHeadPtr FW_Tracking_CreateFromHead;
		FW_Tracking_CreateFromSpawnerPtr FW_Tracking_CreateFromSpawner;
		FW_Tracking_MovePtr FW_Tracking_Move;

		// Fragment merge
		typedef bool(*FW_RefitMerge_InitPtr)();
		typedef void(*FW_RefitMerge_PreparePtr)();
		typedef void(*FW_RefitMerge_ApplyPtr)();

		FW_RefitMerge_InitPtr FW_RefitMerge_Init;
		FW_RefitMerge_PreparePtr FW_RefitMerge_Prepare;
		FW_RefitMerge_ApplyPtr FW_RefitMerge_Apply;

		// Fragment merge: Adjustments query
		typedef int(*FW_RefitMerge_GetNumAdjustedFragmentsPtr)();
		typedef int(*FW_RefitMerge_GetAdjustedFragmentsPtr)(int adjustedFragmentsBufferSize, FrozenWorld_RefitMerge_AdjustedFragment* adjustedFragmentsOut);
		typedef int(*FW_RefitMerge_GetAdjustedAnchorIdsPtr)(FrozenWorld_FragmentId fragmentId, int adjustedAnchorIdsBufferSize, FrozenWorld_AnchorId* adjustedAnchorIdsOut);
		typedef void(*FW_RefitMerge_GetMergedFragmentIdPtr)(FrozenWorld_FragmentId* mergedFragmentIdOut);

		FW_RefitMerge_GetNumAdjustedFragmentsPtr FW_RefitMerge_GetNumAdjustedFragments;
		FW_RefitMerge_GetAdjustedFragmentsPtr FW_RefitMerge_GetAdjustedFragments;
		FW_RefitMerge_GetAdjustedAnchorIdsPtr FW_RefitMerge_GetAdjustedAnchorIds;
		FW_RefitMerge_GetMergedFragmentIdPtr FW_RefitMerge_GetMergedFragmentId;

		// Refreeze
		typedef bool(*FW_RefitRefreeze_InitPtr)();
		typedef void(*FW_RefitRefreeze_PreparePtr)();
		typedef void(*FW_RefitRefreeze_ApplyPtr)();

		FW_RefitRefreeze_InitPtr FW_RefitRefreeze_Init;
		FW_RefitRefreeze_PreparePtr FW_RefitRefreeze_Prepare;
		FW_RefitRefreeze_ApplyPtr FW_RefitRefreeze_Apply;

		// Refreeze: Adjustments query
		typedef int(*FW_RefitRefreeze_GetNumAdjustedFragmentsPtr)();
		typedef int(*FW_RefitRefreeze_GetNumAdjustedAnchorsPtr)();
		typedef int(*FW_RefitRefreeze_GetAdjustedFragmentIdsPtr)(int adjustedFragmentIdsBufferSize, FrozenWorld_FragmentId* adjustedFragmentIdsOut);
		typedef int(*FW_RefitRefreeze_GetAdjustedAnchorIdsPtr)(int adjustedAnchorIdsBufferSize, FrozenWorld_AnchorId* adjustedAnchorIdsOut);
		typedef bool(*FW_RefitRefreeze_CalcAdjustmentPtr)(FrozenWorld_AttachmentPoint* attachmentPointInOut, FrozenWorld_Transform* objectAdjustmentOut);
		typedef void(*FW_RefitRefreeze_GetMergedFragmentIdPtr)(FrozenWorld_FragmentId* mergedFragmentIdOut);

		FW_RefitRefreeze_GetNumAdjustedFragmentsPtr FW_RefitRefreeze_GetNumAdjustedFragments;
		FW_RefitRefreeze_GetNumAdjustedAnchorsPtr FW_RefitRefreeze_GetNumAdjustedAnchors;
		FW_RefitRefreeze_GetAdjustedFragmentIdsPtr FW_RefitRefreeze_GetAdjustedFragmentIds;
		FW_RefitRefreeze_GetAdjustedAnchorIdsPtr FW_RefitRefreeze_GetAdjustedAnchorIds;
		FW_RefitRefreeze_CalcAdjustmentPtr FW_RefitRefreeze_CalcAdjustment;
		FW_RefitRefreeze_GetMergedFragmentIdPtr FW_RefitRefreeze_GetMergedFragmentId;

		// Persistence: Serialization
		typedef void(*FW_Serialize_OpenPtr)(FrozenWorld_Serialize_Stream* streamInOut);
		typedef void(*FW_Serialize_GatherPtr)(FrozenWorld_Serialize_Stream* streamInOut);
		typedef int(*FW_Serialize_ReadPtr)(FrozenWorld_Serialize_Stream* streamInOut, int bytesBufferSize, char* bytesOut);
		typedef void(*FW_Serialize_ClosePtr)(FrozenWorld_Serialize_Stream* streamInOut);

		FW_Serialize_OpenPtr FW_Serialize_Open;
		FW_Serialize_GatherPtr FW_Serialize_Gather;
		FW_Serialize_ReadPtr FW_Serialize_Read;
		FW_Serialize_ClosePtr FW_Serialize_Close;

		// Persistence: Deserialization
		typedef void(*FW_Deserialize_OpenPtr)(FrozenWorld_Deserialize_Stream* streamInOut);
		typedef int(*FW_Deserialize_WritePtr)(FrozenWorld_Deserialize_Stream* streamInOut, int numBytes, char* bytes);
		typedef void(*FW_Deserialize_ApplyPtr)(FrozenWorld_Deserialize_Stream* streamInOut);
		typedef void(*FW_Deserialize_ClosePtr)(FrozenWorld_Deserialize_Stream* streamInOut);

		FW_Deserialize_OpenPtr FW_Deserialize_Open;
		FW_Deserialize_WritePtr FW_Deserialize_Write;
		FW_Deserialize_ApplyPtr FW_Deserialize_Apply;
		FW_Deserialize_ClosePtr FW_Deserialize_Close;
	};
}