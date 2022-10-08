// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <cassert>
#include <cstdbool>
#include <cstdint>

#ifndef HK_FROZENWORLD_EXPORT_DECL
#	if defined(__GNUC__) || defined(__clang__)
#		define HK_FROZENWORLD_EXPORT_DECL __attribute__((visibility("default")))
#		define HK_FROZENWORLD_EXPORT_ENCLOSING_FUNCTION
#	elif defined(_MSC_VER)
#		define HK_FROZENWORLD_EXPORT_DECL  // cannot use __declspec(dllexport) because that exports the decorated name
#		define HK_FROZENWORLD_EXPORT_ENCLOSING_FUNCTION __pragma(comment(linker, "/export:" __FUNCTION__ "=" __FUNCDNAME__))
#	endif
#endif

#ifndef HK_FROZENWORLD_CALL
#	if defined _MSC_VER
#		define HK_FROZENWORLD_CALL __stdcall
#	else
#		define HK_FROZENWORLD_CALL
#	endif
#endif

#ifndef HK_FROZENWORLD_FUNCTION
#	define HK_FROZENWORLD_FUNCTION(RETURN_TYPE, FUNCTION_NAME) \
		HK_FROZENWORLD_EXPORT_DECL RETURN_TYPE HK_FROZENWORLD_CALL FUNCTION_NAME
#endif

extern "C"
{
	static_assert(sizeof(bool) == 1, "require 8-bit bool");
	static_assert(sizeof(int) == 4, "require 32-bit int");
	static_assert(sizeof(float) == 4, "require 32-bit float");

	typedef uint64_t FrozenWorld_AnchorId;
	typedef uint64_t FrozenWorld_FragmentId;

	// Special values for FrozenWorld_AnchorId
	static const FrozenWorld_AnchorId FrozenWorld_AnchorId_INVALID = 0;
	static const FrozenWorld_AnchorId FrozenWorld_AnchorId_UNKNOWN = 0xFFFFFFFFFFFFFFFF;

	// Special values for FrozenWorld_FragmentId
	static const FrozenWorld_FragmentId FrozenWorld_FragmentId_INVALID = 0;
	static const FrozenWorld_FragmentId FrozenWorld_FragmentId_UNKNOWN = 0xFFFFFFFFFFFFFFFF;

	struct FrozenWorld_Vector
	{
		float x;
		float y;
		float z;
	};

	struct FrozenWorld_Quaternion
	{
		float x;
		float y;
		float z;
		float w;
	};

	struct FrozenWorld_Transform
	{
		FrozenWorld_Vector position;
		FrozenWorld_Quaternion rotation;
	};

	struct FrozenWorld_AttachmentPoint
	{
		FrozenWorld_AnchorId anchorId;
		FrozenWorld_Vector locationFromAnchor;
	};

	struct FrozenWorld_AlignConfig
	{
		// Max edge deviation (0.0..1.0, default 0.05) to cut off significantly deviating anchors from alignment
		float edgeDeviationThreshold;

		// Relevance gradient away from head
		float relevanceSaturationRadius;  // 1.0 at this distance from head
		float relevanceDropoffRadius;	 // 0.0 at this distance (must be greater than saturation radius)

		// Tightness gradient away from head
		float tightnessSaturationRadius;  // 1.0 at this distance from head
		float tightnessDropoffRadius;	 // 0.0 at this distance (must be greater than saturation radius)
	};

	struct FrozenWorld_Support
	{
		FrozenWorld_AttachmentPoint attachmentPoint;

		float relevance;  // 1.0 (max) .. 0.0 (min, excluded)
		float tightness;  // 1.0 (max) .. 0.0 (min, only lateral alignment)
	};

	enum FrozenWorld_Snapshot
	{
		FrozenWorld_Snapshot_SPONGY = 0,
		FrozenWorld_Snapshot_FROZEN = 1,
		FrozenWorld_Snapshot_CUSTOM = 1000,
	};

	struct FrozenWorld_Anchor
	{
		FrozenWorld_AnchorId anchorId;
		FrozenWorld_FragmentId fragmentId;
		FrozenWorld_Transform transform;
	};

	struct FrozenWorld_Edge
	{
		FrozenWorld_AnchorId anchorId1;
		FrozenWorld_AnchorId anchorId2;
	};

	struct FrozenWorld_Metrics
	{
		// Merge and refreeze indicators
		bool refitMergeIndicated;
		bool refitRefreezeIndicated;  // configurable

		// Currently trackable fragments
		int numTrackableFragments;

		// Alignment supports
		int numVisualSupports;
		int numVisualSupportAnchors;
		int numIgnoredSupports;
		int numIgnoredSupportAnchors;

		// Visual deviation metrics
		float maxLinearDeviation;
		float maxLateralDeviation;
		float maxAngularDeviation;		   // configurable
		float maxLinearDeviationInFrustum;   // configurable
		float maxLateralDeviationInFrustum;  // configurable
		float maxAngularDeviationInFrustum;  // configurable
	};

	struct FrozenWorld_MetricsConfig
	{
		// Angular deviation capped to this distance
		float angularDeviationNearDistance;

		// View frustum
		float frustumHorzAngle;
		float frustumVertAngle;

		// Thresholds for refreeze indicator
		float refreezeLinearDeviationThreshold;
		float refreezeLateralDeviationThreshold;
		float refreezeAngularDeviationThreshold;
	};

	struct FrozenWorld_RefitMerge_AdjustedFragment
	{
		FrozenWorld_FragmentId fragmentId;
		int numAdjustedAnchors;
		FrozenWorld_Transform adjustment;  // post-merged from pre-merged
	};

	struct FrozenWorld_Serialize_Stream
	{
		// Internal handle to this serialization stream
		int handle;

		// Number of bytes that at least remain to be serialized for a complete record
		int numBytesBuffered;

		// Real time in seconds serialized into this stream so far
		// (can be modified to control relative timestamps serialized into the stream)
		float time;

		// Selection of data to include in the stream
		// (can be modified to control what is serialized into the stream)
		bool includePersistent;  // frozen anchors and edges
		bool includeTransient;   // alignment config, all other snapshot data, supports
	};

	struct FrozenWorld_Deserialize_Stream
	{
		// Internal handle to this deserialization stream
		int handle;

		// Number of bytes that at least remain to be deserialized for a complete record
		int numBytesRequired;

		// Real time in seconds deserialized from this stream so far
		// (can be modified to change its base value for subsequent deserialized records)
		float time;

		// Selection of data applied from the stream
		// (can be modified to control what is deserialized from the stream)
		bool includePersistent;  // frozen anchors and edges
		bool includeTransient;   // alignment config, all other snapshot data, supports
	};

	// clang-format off

	// Version
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_GetVersion)(bool detail, int versionBufferSize, char* versionOut);

	// Errors and diagnostics
	HK_FROZENWORLD_FUNCTION(bool, FrozenWorld_GetError)();
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_GetErrorMessage)(int messageBufferSize, char* messageOut);

	// Internal testing
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_ThrowInternalError)();

	// Startup and teardown
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Init)();
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Destroy)();

	// Alignment
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Step_Init)();
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_Step_GatherSupports)();
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Step_AlignSupports)();

	// Alignment configuration
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_GetAlignConfig)(FrozenWorld_AlignConfig* configOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_SetAlignConfig)(FrozenWorld_AlignConfig* config);

	// Supports access
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_GetNumSupports)();
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_GetSupports)(int supportsBufferSize, FrozenWorld_Support* supportsOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_SetSupports)(int numSupports, FrozenWorld_Support* supports);

	// Snapshot access: Head and alignment
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_GetHead)(FrozenWorld_Snapshot snapshot, FrozenWorld_Vector* headPositionOut, FrozenWorld_Vector* headDirectionForwardOut, FrozenWorld_Vector* headDirectionUpOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_SetHead)(FrozenWorld_Snapshot snapshot, FrozenWorld_Vector* headPosition, FrozenWorld_Vector* headDirectionForward, FrozenWorld_Vector* headDirectionUp);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_GetAlignment)(FrozenWorld_Transform* spongyFromFrozenTransformOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_SetAlignment)(FrozenWorld_Transform* spongyFromFrozenTransform);

	// Snapshot access: Most significant anchor
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_GetMostSignificantAnchorId)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId* anchorIdOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_SetMostSignificantAnchorId)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId anchorId);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_GetMostSignificantFragmentId)(FrozenWorld_Snapshot snapshot, FrozenWorld_FragmentId* fragmentIdOut);

	// Snapshot access: Anchors
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_GetNumAnchors)(FrozenWorld_Snapshot snapshot);
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_GetAnchors)(FrozenWorld_Snapshot snapshot, int anchorsBufferSize, FrozenWorld_Anchor* anchorsOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_AddAnchors)(FrozenWorld_Snapshot snapshot, int numAnchors, FrozenWorld_Anchor* anchors);
	HK_FROZENWORLD_FUNCTION(bool, FrozenWorld_SetAnchorTransform)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId anchorId, FrozenWorld_Transform* transform);
	HK_FROZENWORLD_FUNCTION(bool, FrozenWorld_SetAnchorFragment)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId anchorId, FrozenWorld_FragmentId fragmentId);
	HK_FROZENWORLD_FUNCTION(bool, FrozenWorld_RemoveAnchor)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId anchorId);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_ClearAnchors)(FrozenWorld_Snapshot snapshot);

	// Snapshot access: Edges
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_GetNumEdges)(FrozenWorld_Snapshot snapshot);
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_GetEdges)(FrozenWorld_Snapshot snapshot, int edgesBufferSize, FrozenWorld_Edge* edgesOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_AddEdges)(FrozenWorld_Snapshot snapshot, int numEdges, FrozenWorld_Edge* edges);
	HK_FROZENWORLD_FUNCTION(bool, FrozenWorld_RemoveEdge)(FrozenWorld_Snapshot snapshot, FrozenWorld_AnchorId anchorId1, FrozenWorld_AnchorId anchorId2);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_ClearEdges)(FrozenWorld_Snapshot snapshot);

	// Snapshot access: Utilities
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_MergeAnchorsAndEdges)(FrozenWorld_Snapshot sourceSnapshot, FrozenWorld_Snapshot targetSnapshot);
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_GuessMissingEdges)(FrozenWorld_Snapshot snapshot, int guessedEdgesBufferSize, FrozenWorld_Edge* guessedEdgesOut);

	// Metrics
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_GetMetrics)(FrozenWorld_Metrics* metricsOut);

	// Metrics configuration
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_GetMetricsConfig)(FrozenWorld_MetricsConfig* configOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_SetMetricsConfig)(FrozenWorld_MetricsConfig* config);

	// Scene object tracking
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Tracking_CreateFromHead)(FrozenWorld_Vector* frozenLocation, FrozenWorld_AttachmentPoint* attachmentPointOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Tracking_CreateFromSpawner)(FrozenWorld_AttachmentPoint* spawnerAttachmentPoint, FrozenWorld_Vector* frozenLocation, FrozenWorld_AttachmentPoint* attachmentPointOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Tracking_Move)(FrozenWorld_Vector* targetFrozenLocation, FrozenWorld_AttachmentPoint* attachmentPointInOut);

	// Fragment merge
	HK_FROZENWORLD_FUNCTION(bool, FrozenWorld_RefitMerge_Init)();
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_RefitMerge_Prepare)();
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_RefitMerge_Apply)();

	// Fragment merge: Adjustments query
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_RefitMerge_GetNumAdjustedFragments)();
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_RefitMerge_GetAdjustedFragments)(int adjustedFragmentsBufferSize, FrozenWorld_RefitMerge_AdjustedFragment* adjustedFragmentsOut);
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_RefitMerge_GetAdjustedAnchorIds)(FrozenWorld_FragmentId fragmentId, int adjustedAnchorIdsBufferSize, FrozenWorld_AnchorId* adjustedAnchorIdsOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_RefitMerge_GetMergedFragmentId)(FrozenWorld_FragmentId* mergedFragmentIdOut);

	// Refreeze
	HK_FROZENWORLD_FUNCTION(bool, FrozenWorld_RefitRefreeze_Init)();
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_RefitRefreeze_Prepare)();
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_RefitRefreeze_Apply)();

	// Refreeze: Adjustments query
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_RefitRefreeze_GetNumAdjustedFragments)();
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_RefitRefreeze_GetNumAdjustedAnchors)();
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_RefitRefreeze_GetAdjustedFragmentIds)(int adjustedFragmentIdsBufferSize, FrozenWorld_FragmentId* adjustedFragmentIdsOut);
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_RefitRefreeze_GetAdjustedAnchorIds)(int adjustedAnchorIdsBufferSize, FrozenWorld_AnchorId* adjustedAnchorIdsOut);
	HK_FROZENWORLD_FUNCTION(bool, FrozenWorld_RefitRefreeze_CalcAdjustment)(FrozenWorld_AttachmentPoint* attachmentPointInOut, FrozenWorld_Transform* objectAdjustmentOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_RefitRefreeze_GetMergedFragmentId)(FrozenWorld_FragmentId* mergedFragmentIdOut);

	// Persistence: Serialization
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Serialize_Open)(FrozenWorld_Serialize_Stream* streamInOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Serialize_Gather)(FrozenWorld_Serialize_Stream* streamInOut);
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_Serialize_Read)(FrozenWorld_Serialize_Stream* streamInOut, int bytesBufferSize, char* bytesOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Serialize_Close)(FrozenWorld_Serialize_Stream* streamInOut);

	// Persistence: Deserialization
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Deserialize_Open)(FrozenWorld_Deserialize_Stream* streamInOut);
	HK_FROZENWORLD_FUNCTION(int, FrozenWorld_Deserialize_Write)(FrozenWorld_Deserialize_Stream* streamInOut, int numBytes, char* bytes);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Deserialize_Apply)(FrozenWorld_Deserialize_Stream* streamInOut);
	HK_FROZENWORLD_FUNCTION(void, FrozenWorld_Deserialize_Close)(FrozenWorld_Deserialize_Stream* streamInOut);
}
