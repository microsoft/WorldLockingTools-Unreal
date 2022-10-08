// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "CoreMinimal.h"
#include "FrozenWorldPlugin.h"
#include "FrozenWorldPoseExtensions.h"
#include "Misc/AutomationTest.h"

namespace WorldLockingTools
{
	class FWLTTests
	{
	private:
		struct FPinData
		{
		public:
			FString name;
			FTransform virtualPose;
			FTransform lockedPose;
		};

		FrozenWorld_AnchorId MakeAnchorId(int idx)
		{
			return FrozenWorld_AnchorId_INVALID + 1 + idx;
		}

		bool CheckAlignment(TArray<FrozenWorld_Anchor> anchorPoses, TArray<FrozenWorld_Edge> anchorEdges, FTransform movement)
		{
			FTransform spongyHead;
			FFrozenWorldPlugin::Get()->GetFrozenWorldInterop().ResetAlignment(FTransform::Identity);
			for (int k = 0; k < anchorPoses.Num(); ++k)
			{
				spongyHead = FFrozenWorldPlugin::Get()->GetFrozenWorldInterop().FtoU(anchorPoses[k].transform);
				FFrozenWorldPlugin::Get()->ClearSpongyAnchors();
				FFrozenWorldPlugin::Get()->Step_Init(spongyHead);
				FFrozenWorldPlugin::Get()->AddSpongyAnchors(anchorPoses);
				FFrozenWorldPlugin::Get()->SetMostSignificantSpongyAnchorId(anchorPoses[k].anchorId);
				FFrozenWorldPlugin::Get()->AddSpongyEdges(anchorEdges);
				FFrozenWorldPlugin::Get()->Step_Finish();

				auto adjustment = FFrozenWorldPlugin::Get()->GetFrozenWorldInterop().GetAlignment();

				if (!adjustment.Equals(movement))
				{
					UE_LOG(LogHMD, Error, TEXT("k=%d adjustment=%s, movement=%s"), k, *adjustment.ToString(), *movement.ToString());
					return false;
				}
			}

			return true;
		}

		bool CheckAlignment(FVector virtualPos, FVector lockedPos)
		{
			FAlignmentManager::Get()->ComputePinnedPose(FTransform(FQuat::Identity, lockedPos));
			FTransform PinnedFromLocked = FAlignmentManager::Get()->PinnedFromLocked;
			FTransform FrozenFromLocked = FFrozenWorldPoseExtensions::Multiply(FFrozenWorldPlugin::Get()->FrozenFromPinned(), PinnedFromLocked);
			FTransform LockedFromFrozen = FFrozenWorldPoseExtensions::Inverse(FrozenFromLocked);

			FVector computedLocked = LockedFromFrozen.TransformPosition(virtualPos);
			bool areEqual = computedLocked == lockedPos;
			check(areEqual);

			return areEqual;
		}

		void PreMultiplyPoses(TArray<FrozenWorld_Anchor>& dstPoses, TArray<FrozenWorld_Anchor> srcPoses, FTransform transform)
		{
			check(dstPoses.Num() == srcPoses.Num());

			for (int i = 0; i < srcPoses.Num(); ++i)
			{
				FrozenWorld_Anchor anchorPose = dstPoses[i];
				anchorPose.transform = FFrozenWorldInterop::UtoF(FFrozenWorldInterop::FtoU(srcPoses[i].transform) * transform);
				dstPoses[i] = anchorPose;
			}
		}

		bool FloatCompare(float lhs, float rhs, float eps = -1.0f)
		{
			if (eps < 0)
			{
				eps = 1.0e-6f;
			}
			return lhs + eps >= rhs && lhs <= rhs + eps;
		}

		bool CheckWeight(Interpolant interp, int idx, float weight)
		{
			int found = 0;
			for (int i = 0; i < 3; ++i)
			{
				if (interp.idx[i] == idx && interp.weights[i] > 0)
				{
					check(FloatCompare(interp.weights[i], weight) == true);
					++found;
				}
			}
			// If the input weight is non-zero, there should be exactly one non-zero weight occurrence of it.
			return ((weight == 0 && found == 0) || (weight > 0 && found == 1));
		}

		bool CheckWeightZero(Interpolant interp, int idx)
		{
			bool succeeded = true;
			for (int i = 0; i < 3; ++i)
			{
				if (interp.idx[i] == idx)
				{
					succeeded &= (interp.weights[i] == 0);
				}
			}

			return succeeded;
		}
		bool CheckWeightsZero(Interpolant interp, TArray<int> idx)
		{
			bool succeeded = true;
			for (int i = 0; i < idx.Num(); ++i)
			{
				succeeded &= CheckWeightZero(interp, idx[i]);
			}

			return succeeded;
		}

		TArray<int> AllButOne(int count, int excluded)
		{
			TArray<int> indices;
			for (int i = 0; i < count; ++i)
			{
				if (i != excluded)
				{
					indices.Add(i);
				}
			}
			return indices;
		}

		bool CheckVertices(FTriangulator triangulator, TArray<FVector> vertices)
		{
			bool succeeded = true;
			Interpolant interp;
			for (int i = 0; i < vertices.Num(); ++i)
			{
				triangulator.Find(vertices[i], interp);
				succeeded &= CheckWeight(interp, i, 1);
				succeeded &= CheckWeightsZero(interp, AllButOne(vertices.Num(), i));

				int next = (i + 1) % vertices.Num();
				FVector midPoint = (vertices[i] + vertices[next]) * 0.5f;
				triangulator.Find(midPoint, interp);
				succeeded &= CheckWeight(interp, i, 0.5f);
				succeeded &= CheckWeight(interp, next, 0.5f);
			}

			return succeeded;
		}

		TArray<FPinData> GetPinData()
		{
			TArray<FPinData> PinData;

			PinData.Add(FPinData
				{
					FString("pin0"),
					FTransform(FQuat::Identity, FVector::ZeroVector),
					FTransform(FQuat::Identity, FVector::ZeroVector)
				});

			PinData.Add(FPinData
				{
					FString("pin1"),
					FTransform(FQuat::Identity, FVector(0, 100, 0)),
					FTransform(FQuat::Identity, FVector(0, 200, 0))
				});

			PinData.Add(FPinData
				{
					FString("pin2"),
					FTransform(FQuat::Identity, FVector(100, 100, 0)),
					FTransform(FQuat::Identity, FVector(200, 200, 0))
				});

			PinData.Add(FPinData
				{
					FString("pin3"),
					FTransform(FQuat::Identity, FVector(100, 0, 0)),
					FTransform(FQuat::Identity, FVector(200, 0, 0))
				});

			return PinData;
		}

		bool CheckSinglePin(int pinIdx)
		{
			bool testPassed = true;
			FAlignmentManager::Get()->ClearAlignmentAnchors();

			TArray<FPinData> pinData = GetPinData();
			auto id0 = FAlignmentManager::Get()->AddAlignmentAnchor(pinData[pinIdx].name, pinData[pinIdx].virtualPose, pinData[pinIdx].lockedPose);
			FAlignmentManager::Get()->SendAlignmentAnchors();

			testPassed &= CheckAlignment(pinData[pinIdx].virtualPose.GetLocation(), pinData[pinIdx].lockedPose.GetLocation());
			testPassed &= CheckAlignment(pinData[pinIdx].virtualPose.GetLocation() + FVector(0, 100, 0), pinData[pinIdx].lockedPose.GetLocation() + FVector(0, 100, 0));

			FAlignmentManager::Get()->ClearAlignmentAnchors();
			FAlignmentManager::Get()->SendAlignmentAnchors();

			return testPassed;
		}

		bool CheckDualPins(int pinIdx0, int pinIdx1)
		{
			bool testPassed = true;

			TArray<FPinData> pinData = GetPinData();
			FAlignmentManager::Get()->AddAlignmentAnchor(pinData[pinIdx0].name, pinData[pinIdx0].virtualPose, pinData[pinIdx0].lockedPose);
			FAlignmentManager::Get()->AddAlignmentAnchor(pinData[pinIdx1].name, pinData[pinIdx1].virtualPose, pinData[pinIdx1].lockedPose);
			FAlignmentManager::Get()->SendAlignmentAnchors();

			testPassed &= CheckAlignment(pinData[pinIdx0].virtualPose.GetLocation(), pinData[pinIdx0].lockedPose.GetLocation());
			testPassed &= CheckAlignment(pinData[pinIdx1].virtualPose.GetLocation(), pinData[pinIdx1].lockedPose.GetLocation());

			testPassed &= CheckAlignment(
				(pinData[pinIdx0].virtualPose.GetLocation() + pinData[pinIdx1].virtualPose.GetLocation()) * 0.5f,
				(pinData[pinIdx0].lockedPose.GetLocation() + pinData[pinIdx1].lockedPose.GetLocation()) * 0.5f);

			FAlignmentManager::Get()->ClearAlignmentAnchors();
			FAlignmentManager::Get()->SendAlignmentAnchors();
			return testPassed;
		}

	public:
		bool RunTestWorldLockingManagerGraph()
		{
			bool testPassed = true;
			FFrozenWorldPlugin::Get()->GetFrozenWorldInterop().ClearFrozenAnchors();

			TArray<FTransform> poses;
			poses.Add(FTransform(FQuat::Identity, FVector(0, 0, 0)));
			poses.Add(FTransform(FQuat::Identity, FVector(0, 300, 0)));
			poses.Add(FTransform(FQuat::Identity, FVector(300, 300, 0)));
			poses.Add(FTransform(FQuat::Identity, FVector(300, 0, 0)));

			TArray<FrozenWorld_Anchor> anchorPoses;
			for (int i = 0; i < poses.Num(); ++i)
			{
				anchorPoses.Add(FrozenWorld_Anchor{ MakeAnchorId(i), FrozenWorld_FragmentId_UNKNOWN, FFrozenWorldInterop::UtoF(poses[i]) });
			}

			FTransform spongyHead = FTransform::Identity;
			FTransform adjustment = FTransform::Identity;
			TArray<FrozenWorld_Edge> anchorEdges;
			for (int i = 0; i < anchorPoses.Num(); ++i)
			{
				for (int j = i + 1; j < anchorPoses.Num(); ++j)
				{
					anchorEdges.Add(FrozenWorld_Edge{ anchorPoses[i].anchorId, anchorPoses[j].anchorId });
				}
			}

			FTransform movement = FTransform::Identity;
			TArray<FrozenWorld_Anchor> displacedPoses;
			for (int i = 0; i < anchorPoses.Num(); ++i)
			{
				displacedPoses.Add(anchorPoses[i]);
			}
			testPassed &= CheckAlignment(displacedPoses, anchorEdges, movement);

			/// Take a random walk.
			int numRandomSteps = 100;
			for (int i = 0; i < numRandomSteps; ++i)
			{
				FVector randomStep = FVector(FMath::FRandRange(-0.1f, 0.1f), FMath::FRandRange(-0.1f, 0.1f), FMath::FRandRange(-0.1f, 0.1f));
				FTransform step = FTransform(FQuat::Identity, randomStep);
				movement *= step;
				PreMultiplyPoses(displacedPoses, anchorPoses, movement);
				testPassed &= CheckAlignment(displacedPoses, anchorEdges, movement);
			}

			/// Now walk back to start.
			FTransform furthest = movement;
			for (int i = 0; i < numRandomSteps; ++i)
			{
				//FVector:: Vector3.Lerp(furthest.position, Vector3.zero, (float)i / (float)(numRandomSteps - 1));
				float x = FMath::Lerp(furthest.GetLocation().X, 0, (float)i / (float)(numRandomSteps - 1));
				float y = FMath::Lerp(furthest.GetLocation().Y, 0, (float)i / (float)(numRandomSteps - 1));
				float z = FMath::Lerp(furthest.GetLocation().Z, 0, (float)i / (float)(numRandomSteps - 1));
				movement.SetLocation(FVector(x, y, z));
				PreMultiplyPoses(displacedPoses, anchorPoses, movement);
				testPassed &= CheckAlignment(displacedPoses, anchorEdges, movement);
			}

			/// Try incremental rotation.
			int numRotSteps = 10;
			FTransform rotStep = FTransform(FQuat(FVector::UpVector, 1.0f), FVector::ZeroVector);
			for (int i = 0; i < numRotSteps; ++i)
			{
				movement *= rotStep;
				PreMultiplyPoses(displacedPoses, anchorPoses, movement);
				testPassed &= CheckAlignment(displacedPoses, anchorEdges, movement);
			}

			return testPassed;
		}

		bool RunTestTriangulatorSquare()
		{
			bool succeeded = true;

			FTriangulator triangulator;
			triangulator.SetBounds(FVector(-100000, -100000, 0), FVector(100000, 100000, 0));

			TArray<FVector> vertices;
			vertices.Add(FVector(-100, -100, 0));
			vertices.Add(FVector(-100, 100, 0));
			vertices.Add(FVector(100, 100, 0));
			vertices.Add(FVector(100, -100, 0));

			triangulator.Add(vertices);
			succeeded &= CheckVertices(triangulator, vertices);

			Interpolant interp;

			FVector center = FVector::ZeroVector;
			for (int i = 0; i < vertices.Num(); ++i)
			{
				center += vertices[i];
			}
			center *= 1.0f / vertices.Num();
			triangulator.Find(center, interp);
			// Check that the interpolation is the center of either the diagonal formed by vert[0]-vert[2]
			// or the diagonal vert[1]-vert[3].
			TArray<float> wgts;
			wgts.AddDefaulted(vertices.Num());
			for (int i = 0; i < 3; ++i)
			{
				if (interp.weights[i] > 0)
				{
					wgts[interp.idx[i]] = interp.weights[i];
				}
			}
			float eps = 1.0e-4f;
			bool diag02 = FloatCompare(wgts[0], 0.5f, eps) && FloatCompare(wgts[1], 0, eps) && FloatCompare(wgts[2], 0.5f, eps) && FloatCompare(wgts[3], 0, eps);
			bool diag13 = FloatCompare(wgts[0], 0, eps) && FloatCompare(wgts[1], 0.5f, eps) && FloatCompare(wgts[2], 0, eps) && FloatCompare(wgts[3], 0.5f, eps);

			succeeded &= ((diag02 || diag13) && !(diag02 && diag13));

			return succeeded;
		}

		bool RunTestTriangulatorLine()
		{
			bool succeeded = true;

			FTriangulator triangulator;
			triangulator.SetBounds(FVector(-100000, -100000, 0), FVector(100000, 100000, 0));

			TArray<FVector> vertices;
			vertices.Add(FVector(130, -130, 0));
			vertices.Add(FVector(130, -30, 0));
			vertices.Add(FVector(130, 30, 0));
			vertices.Add(FVector(130, 130, 0));
			vertices.Add(FVector(130, 200, 0));
			vertices.Add(FVector(130, 300, 0));
			vertices.Add(FVector(130, 400, 0));

			triangulator.Add(vertices);

			Interpolant interp;

			for (int i = 0; i < vertices.Num(); ++i)
			{
				triangulator.Find(vertices[i], interp);
				succeeded &= CheckWeight(interp, i, 1.0f);
				succeeded &= CheckWeightsZero(interp, AllButOne(vertices.Num(), i));
			}

			for (int i = 0; i < vertices.Num(); ++i)
			{
				triangulator.Find(vertices[i] + FVector(0.0f, 0.0f, -170), interp);
				succeeded &= CheckWeight(interp, i, 1.0f);
				succeeded &= CheckWeightsZero(interp, AllButOne(vertices.Num(), i));
			}

			for (int i = 1; i < vertices.Num(); ++i)
			{
				FVector midPoint = (vertices[i] + vertices[i - 1]) * 0.5f;
				triangulator.Find(midPoint, interp);
				succeeded &= CheckWeight(interp, i, 0.5f);
				succeeded &= CheckWeight(interp, i - 1, 0.5f);
			}

			for (int i = 1; i < vertices.Num(); ++i)
			{
				FVector midPoint = (vertices[i] + vertices[i - 1]) * 0.5f + FVector(0.0f, 0.0f, 110);
				triangulator.Find(midPoint, interp);
				succeeded &= CheckWeight(interp, i, 0.5f);
				succeeded &= CheckWeight(interp, i - 1, 0.5f);
			}

			return succeeded;
		}

		bool RunTestTriangulatorObtuse()
		{
			bool succeeded = true;

			FTriangulator triangulator;
			triangulator.SetBounds(FVector(-100000, -100000, 0), FVector(100000, 100000, 0));

			TArray<FVector> vertices;
			vertices.Add(FVector::ZeroVector);
			vertices.Add(FVector(0, 200, 0));
			vertices.Add(FVector(4, 100, 0));

			triangulator.Add(vertices);

			Interpolant interp;
			triangulator.Find(FVector::ZeroVector, interp);
			succeeded &= CheckWeight(interp, 0, 1);
			TArray<int> idx;
			idx.Add(1);
			idx.Add(2);
			succeeded &= CheckWeightsZero(interp, idx);

			triangulator.Find(FVector(0, 200, 0), interp);
			succeeded &= CheckWeight(interp, 1, 1);
			idx.Empty();
			idx.Add(0);
			idx.Add(2);
			succeeded &= CheckWeightsZero(interp, idx);

			/// This one is outside the triangle, but should interpolate to the obtuse vertex exactly.
			triangulator.Find(FVector(20, 100, 0), interp);
			succeeded &= CheckWeight(interp, 2, 1);
			idx.Empty();
			idx.Add(0);
			idx.Add(1);
			succeeded &= CheckWeightsZero(interp, idx);

			return succeeded;
		}

		bool RunTestAlignmentThreeBodyOrient()
		{
			TArray<FVector> modelPositions;
			modelPositions.Add(FVector(0, 1, 0));
			modelPositions.Add(FVector(1, 0, 0));
			modelPositions.Add(FVector(0.5f, 0.3f, 0.4f));
			modelPositions.Add(FVector(10, -8, 7));

			TArray<FVector> frozenPositions;
			frozenPositions.AddDefaulted(modelPositions.Num());

			FQuat initFrozenFromModel = FQuat::MakeFromEuler(FVector(0.0f, 45.0f, 45.0f));

			for (int i = 0; i < modelPositions.Num(); ++i)
			{
				frozenPositions[i] = initFrozenFromModel * modelPositions[i];
			}

			float errorLength = 0;
			for (int i = 1; i < modelPositions.Num(); ++i)
			{
				FQuat frozenFromModelFirst = FQuat::FindBetweenVectors(modelPositions[i - 1], frozenPositions[i - 1]);

				FVector firstAlignedSecond = frozenFromModelFirst * modelPositions[i];

				FVector dir = frozenPositions[i - 1];
				dir.Normalize();
				FVector up = FVector::CrossProduct(frozenPositions[i], dir);
				up.Normalize();
				FVector right = FVector::CrossProduct(dir, up);

				float sinRads = FVector::DotProduct(firstAlignedSecond, up);
				float cosRads = FVector::DotProduct(firstAlignedSecond, right);

				float rotRads = FMath::Atan2(sinRads, cosRads);

				FQuat frozenFromModelSecond = FQuat(dir, rotRads);

				FQuat frozenFromModel = frozenFromModelSecond * frozenFromModelFirst;

				TArray<FVector> checkFrozen;
				checkFrozen.AddDefaulted(modelPositions.Num());
				for (int j = 0; j < modelPositions.Num(); ++j)
				{
					checkFrozen[j] = frozenFromModel * modelPositions[j];
					FVector delErr = checkFrozen[j] - frozenPositions[j];
					float errLen = delErr.Length();
					errorLength += errLen;
				}
			}

			UE_LOG(LogTemp, Log, TEXT("Total error %f"), errorLength);

			return errorLength <= KINDA_SMALL_NUMBER;
		}

		bool RunTestAlignmentManagerBasic()
		{
			bool testPassed = true;

			testPassed &= CheckSinglePin(0);
			testPassed &= CheckSinglePin(1);

			testPassed &= CheckDualPins(0, 1);
			testPassed &= CheckDualPins(1, 2);
			testPassed &= CheckDualPins(0, 2);

			FAlignmentManager::Get()->ClearAlignmentAnchors();
			TArray<FPinData> pinData = GetPinData();
			for (int i = 0; i < 2; ++i)
			{
				FAlignmentManager::Get()->AddAlignmentAnchor(pinData[i].name, pinData[i].virtualPose, pinData[i].lockedPose);
			}
			FAlignmentManager::Get()->SendAlignmentAnchors();

			testPassed &= CheckAlignment(pinData[0].virtualPose.GetLocation(), pinData[0].lockedPose.GetLocation());

			testPassed &= CheckAlignment(pinData[1].virtualPose.GetLocation(), pinData[1].lockedPose.GetLocation());

			testPassed &= CheckAlignment(
				(pinData[0].virtualPose.GetLocation() + pinData[1].virtualPose.GetLocation()) * 0.5f,
				(pinData[0].lockedPose.GetLocation() + pinData[1].lockedPose.GetLocation()) * 0.5f);

			return testPassed;
		}
	};
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLTPluginTest, "WLT.Plugin", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWLTPluginTest::RunTest(FString const& Parameters) {
	WorldLockingTools::FWLTTests Test;
	return Test.RunTestWorldLockingManagerGraph();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLTTriangulatorSquareTest, "WLT.Triangulator.Square", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWLTTriangulatorSquareTest::RunTest(FString const& Parameters) {
	WorldLockingTools::FWLTTests Test;
	return Test.RunTestTriangulatorSquare();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLTTriangulatorLineTest, "WLT.Triangulator.Line", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWLTTriangulatorLineTest::RunTest(FString const& Parameters) {
	WorldLockingTools::FWLTTests Test;
	return Test.RunTestTriangulatorLine();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLTTriangulatorObtuseTest, "WLT.Triangulator.Obtuse", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWLTTriangulatorObtuseTest::RunTest(FString const& Parameters) {
	WorldLockingTools::FWLTTests Test;
	return Test.RunTestTriangulatorObtuse();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLTAlignmentThreeBodyTest, "WLT.Alignment.ThreeBodyOrient", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWLTAlignmentThreeBodyTest::RunTest(FString const& Parameters) {
	WorldLockingTools::FWLTTests Test;
	return Test.RunTestAlignmentThreeBodyOrient();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLTAlignmentBasicTest, "WLT.Alignment.Basic", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWLTAlignmentBasicTest::RunTest(FString const& Parameters) {
	WorldLockingTools::FWLTTests Test;
	return Test.RunTestAlignmentManagerBasic();
}

struct Edge
{
	int idx0;
	int idx1;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLTTriangulatorEdgeSortTest, "WLT.Triangulator.Sort", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWLTTriangulatorEdgeSortTest::RunTest(FString const& Parameters) {
	TArray<Edge> edges;
	TArray<FVector> vertices;

	vertices.Add(FVector::ZeroVector); // 0
	vertices.Add(FVector(1, 0, 0));    // 1
	vertices.Add(FVector(10, 0, 0));   // 2
	vertices.Add(FVector(20, 0, 0));   // 3

	edges.Add(Edge{ 0, 0 });
	edges.Add(Edge{ 0, 3 });
	edges.Add(Edge{ 0, 2 });
	edges.Add(Edge{ 0, 1 });
	edges.Add(Edge{ 0, 2 });

	edges.Sort([vertices](Edge e0, Edge e1)
	{
		return (vertices[e1.idx0] - vertices[e1.idx1]).SizeSquared() <
			(vertices[e0.idx0] - vertices[e0.idx1]).SizeSquared();
	});

	float previousLength = std::numeric_limits<float>::max();
	for (int i = 0; i < edges.Num(); i++)
	{
		float currentLength = (vertices[edges[i].idx0] - vertices[edges[i].idx1]).SizeSquared();
		
		if (previousLength < currentLength)
		{
			return false;
		}
		
		previousLength = currentLength;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLTTriangulatorRemoveRedundantEdges, "WLT.Triangulator.RemoveRedundantEdges", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWLTTriangulatorRemoveRedundantEdges::RunTest(FString const& Parameters) {
	TArray<Edge> edges;
	TArray<FVector> vertices;

	vertices.Add(FVector::ZeroVector); // 0
	vertices.Add(FVector(1, 0, 0));    // 1
	vertices.Add(FVector(10, 0, 0));   // 2
	vertices.Add(FVector(20, 0, 0));   // 3

	edges.Add(Edge{ 1, 0 });
	edges.Add(Edge{ 1, 1 });
	edges.Add(Edge{ 1, 1 }); // redundant
	edges.Add(Edge{ 1, 1 }); // redundant
	edges.Add(Edge{ 1, 2 });
	edges.Add(Edge{ 0, 0 });
	edges.Add(Edge{ 0, 3 });
	edges.Add(Edge{ 0, 2 });
	edges.Add(Edge{ 1, 1 }); // redundant
	edges.Add(Edge{ 0, 1 });
	edges.Add(Edge{ 0, 2 }); // redundant

	edges.Sort([this](Edge e0, Edge e1)
	{
		// Sort by first index first.
		if (e0.idx0 < e1.idx0)
		{
			//return -1;
			return false;
		}
		if (e0.idx0 > e1.idx0)
		{
			//return 1;
			return true;
		}
		// First index equal, sort by second index.
		if (e0.idx1 < e1.idx1)
		{
			//return -1;
			return false;
		}
		if (e0.idx1 > e1.idx1)
		{
			//return 1;
			return true;
		}
		return false;// 0;
	});
	// Note i doesn't reach zero, because edges[i] is compared to edges[i-1].
	for (int i = edges.Num() - 1; i > 0; --i)
	{
		if (edges[i - 1].idx0 == edges[i].idx0 && edges[i - 1].idx1 == edges[i].idx1)
		{
			edges.RemoveAt(i);
		}
	}

	return edges.Num() == 7;
}
