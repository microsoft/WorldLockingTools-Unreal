// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once
#include "CoreMinimal.h"

#include <limits>

namespace WorldLockingTools
{
	class Interpolant
	{
	public:
		int idx[3];
		float weights[3];

		bool IsInterior()
		{
			return weights[0] >= 0 && weights[1] >= 0 && weights[2] >= 0;
		}
	};

	struct Triangle
	{
		int idx0;
		int idx1;
		int idx2;
	};

	struct Edge
	{
		int idx0;
		int idx1;
	};

	class IndexedBary
	{
	public:
		int triangle;
		Interpolant bary;
	};

	struct PointOnEdge
	{
		float parm;
		float distanceSqr;
	};

	class FTriangulator
	{
	private:
		TArray<FVector> vertices;
		TArray<Triangle> triangles;
		TArray<Edge> exteriorEdges;

	public:
		void Clear()
		{
			vertices.Empty();
			triangles.Empty();
			exteriorEdges.Empty();
		}

		void SetBounds(FVector minPos, FVector maxPos)
		{
			// Must set bounds before adding vertices. To update bounds, Clear, SetBounds, Add(vertices) again.
			check(vertices.Num() == 0);
			Clear();
			TArray<FVector> bounds;
			bounds.Add(FVector(minPos.X, maxPos.Y, 0.0f));
			bounds.Add(FVector(minPos.X, minPos.Y, 0.0f));
			bounds.Add(FVector(maxPos.X, minPos.Y, 0.0f));
			bounds.Add(FVector(maxPos.X, maxPos.Y, 0.0f));
			SeedQuad(bounds);
		}

		bool Add(TArray<FVector> inVertices)
		{
			// Must set bounds before adding vertices.
			check(vertices.Num() >= 4);
			for (int i = 0; i < inVertices.Num(); ++i)
			{
				AddVertexSubdividing(inVertices[i]);
			}
			FlipLongEdges();
			FindExteriorEdges();
			return true;
		}

		bool Find(FVector pos, Interpolant& outBary)
		{
			bool HasInterpolant = FindTriangleOrEdgeOrVertex(pos, outBary);
			if (HasInterpolant)
			{
				AdjustForBoundingIndices(outBary);
			}

			return HasInterpolant;
		}

		TArray<int> Triangles()
		{
			TArray<int> tris;
			tris.SetNumUninitialized(triangles.Num() * 3);

			for (int i = 0; i < triangles.Num(); i++)
			{
				tris[i * 3] = triangles[i].idx0;
				tris[i * 3 + 1] = triangles[i].idx1;
				tris[i * 3 + 2] = triangles[i].idx2;
			}
			return tris;
		}

	private:
		void AdjustForBoundingIndices(Interpolant& bary)
		{
			//if (bary != null)
			{
				for (int i = 0; i < 3; ++i)
				{
					if (!IsBoundary(bary.idx[i]))
					{
						bary.idx[i] = bary.idx[i] - 4;
					}
					else
					{
						check(bary.weights[i] == 0.0f);
					}
				}
			}
		}

		bool SeedQuad(TArray<FVector> inVertices)
		{
			Clear();
			for (int i = 0; i < inVertices.Num(); ++i)
			{
				vertices.Add(inVertices[i]);
			}
			for (int idxBase = 0; idxBase < inVertices.Num(); idxBase += 3)
			{
				if (idxBase + 2 < inVertices.Num())
				{
					triangles.Add(MakeTriangle(idxBase + 1, idxBase + 2, idxBase + 0));
				}
				if (idxBase + 3 < inVertices.Num())
				{
					triangles.Add(MakeTriangle(idxBase + 0, idxBase + 2, idxBase + 3));
				}
			}
			return triangles.Num() > 0;
		}

		Triangle MakeTriangle(int idx0, int idx1, int idx2)
		{
			float cross = -FVector::CrossProduct(vertices[idx2] - vertices[idx1], vertices[idx0] - vertices[idx1]).Z;
			check(cross != 0); // Degenerate triangle
			if (cross < 0)
			{
				return Triangle{ idx0, idx2, idx1 };
			}
			return Triangle{ idx0, idx1, idx2 };
		}

		bool WindingCorrect(int triIdx)
		{
			Triangle tri = triangles[triIdx];
			float cross = -FVector::CrossProduct(vertices[tri.idx2] - vertices[tri.idx1], vertices[tri.idx0] - vertices[tri.idx1]).Z;
			return cross > 0;
		}

		void AddVertexSubdividing(FVector vtx)
		{
			vertices.Add(vtx);
			int newVertIdx = vertices.Num() - 1;

			// Find closest triangle
			IndexedBary bary = FindTriangle(vtx);
			check(bary.bary.IsInterior()); // Should be contained by background seed vertices.

			// Find closest edge
			Edge edge = ClosestEdge(bary);

			// Find any other triangle with that edge
			int oppositieTriIdx = FindTriangleWithEdge(edge, bary.triangle);

			bool canSplit = CanSplit(edge, oppositieTriIdx, newVertIdx);

			if (canSplit)
			{
				AddVertexSplitEdge(edge, bary.triangle, oppositieTriIdx, newVertIdx);
			}
			else
			{
				AddVertexMidTriangle(bary.triangle, newVertIdx);
			}
		}

		bool CanSplit(Edge edge, int triIdx, int newVertIdx)
		{
			if (triIdx < 0)
			{
				return false;
			}
			Triangle tri = triangles[triIdx];
			if (!EdgesEqual(edge, tri.idx0, tri.idx1))
			{
				if (IsOutsideEdge(tri.idx0, tri.idx1, newVertIdx))
				{
					return false;
				}
			}
			if (!EdgesEqual(edge, tri.idx1, tri.idx2))
			{
				if (IsOutsideEdge(tri.idx1, tri.idx2, newVertIdx))
				{
					return false;
				}
			}
			if (!EdgesEqual(edge, tri.idx2, tri.idx0))
			{
				if (IsOutsideEdge(tri.idx2, tri.idx0, newVertIdx))
				{
					return false;
				}
			}
			return true;
		}

		bool IsOutsideEdge(int vtx0, int vtx1, int vtxTest)
		{
			float cross = -FVector::CrossProduct(vertices[vtx1] - vertices[vtx0], vertices[vtxTest] - vertices[vtx0]).Z;
			return cross <= 0;
		}

		void AddVertexSplitEdge(Edge edge, int triIdx0, int triIdx1, int newVertIdx)
		{
			SplitEdge(triIdx0, edge, newVertIdx);
			SplitEdge(triIdx1, edge, newVertIdx);
		}

		void AddVertexMidTriangle(int triIdx, int newVertIdx)
		{
			Triangle tri = triangles[triIdx];

			triangles[triIdx] = Triangle
			{
				tri.idx0,
				tri.idx1,
				newVertIdx
			};
			triangles.Add(
				Triangle
				{
					tri.idx1,
					tri.idx2,
					newVertIdx
				}
			);
			triangles.Add(
				Triangle
				{
					tri.idx2,
					tri.idx0,
					newVertIdx
				}
			);
		}

		void SplitEdge(int triIdx, Edge edge, int newVertIdx)
		{
			Triangle tri = triangles[triIdx];
			if (EdgesEqual(edge, tri.idx0, tri.idx1))
			{
				Triangle newTri0 = Triangle
				{
					tri.idx0,
					newVertIdx,
					tri.idx2
				};
				Triangle newTri1 = Triangle
				{
					newVertIdx,
					tri.idx1,
					tri.idx2
				};
				triangles[triIdx] = newTri0;
				triangles.Add(newTri1);
			}
			else if (EdgesEqual(edge, tri.idx1, tri.idx2))
			{
				Triangle newTri0 = Triangle
				{
					tri.idx0,
					tri.idx1,
					newVertIdx
				};
				Triangle newTri1 = Triangle
				{
					newVertIdx,
					tri.idx2,
					tri.idx0
				};
				triangles[triIdx] = newTri0;
				triangles.Add(newTri1);
			}
			else
			{
				check(EdgesEqual(edge, tri.idx2, tri.idx0));
				Triangle newTri0 = Triangle
				{
					newVertIdx,
					tri.idx1,
					tri.idx2
				};
				Triangle newTri1 = Triangle
				{
					newVertIdx,
					tri.idx0,
					tri.idx1
				};
				triangles[triIdx] = newTri0;
				triangles.Add(newTri1);
			}
		}

		TArray<Edge> ListSharedEdges()
		{
			TArray<Edge> edges;
			for (int i = 0; i < triangles.Num(); ++i)
			{
				Triangle tri = triangles[i];
				if (tri.idx0 < tri.idx1)
				{
					edges.Add(Edge{ tri.idx0, tri.idx1 });
				}
				if (tri.idx1 < tri.idx2)
				{
					edges.Add(Edge{ tri.idx1, tri.idx2 });
				}
				if (tri.idx2 < tri.idx0)
				{
					edges.Add(Edge{ tri.idx2, tri.idx0 });
				}
			}
			/// Sort the edges longest to shortest.
			edges.Sort([this](Edge e0, Edge e1)
			{
				return (vertices[e1.idx0] - vertices[e1.idx1]).SizeSquared() <
					(vertices[e0.idx0] - vertices[e0.idx1]).SizeSquared();
			});

			return edges;
		}

		bool IsInsideTriangle(int t0, int t1, int t2, int ttest)
		{
			FVector v0 = vertices[t0];
			FVector v1 = vertices[t1];
			FVector v2 = vertices[t2];
			float area = -FVector::CrossProduct(v2 - v1, v0 - v1).Z;
			float nearIn = -area * 1.0e-4f;
			FVector vt = vertices[ttest];
			/// Note the order swap here to account for this being xz not xy.
			return FVector::CrossProduct(vt - v0, v1 - v0).Z >= nearIn
				&& FVector::CrossProduct(vt - v1, v2 - v1).Z >= nearIn
				&& FVector::CrossProduct(vt - v2, v0 - v2).Z >= nearIn;
		}

		void FlipLongEdges()
		{
			/// Make a list of all unique edges (no duplicates).
			TArray<Edge> edges = ListSharedEdges();

			/// For each edge in the list
			for (int iEdge = 0; iEdge < edges.Num(); ++iEdge)
			{
				Edge edge = edges[iEdge];
				/// Find the two triangles sharing it.
				int tri0 = FindTriangleWithEdge(edge, -1);
				check(tri0 >= 0); // Can't find a triangle with a known edge
				int tri1 = FindTriangleWithEdge(edge, tri0);

				/// If there are two triangles
				if (tri1 >= 0)
				{
					/// Shift the indices to form (i,j,k),(k,j,l) where (j,k) is the edge.
					ShiftTriangles(edge, tri0, tri1);

					Triangle t0 = triangles[tri0];
					Triangle t1 = triangles[tri1];
					if (!IsInsideTriangle(t0.idx0, t0.idx1, t1.idx2, t0.idx2)
						&& !IsInsideTriangle(t0.idx0, t1.idx2, t0.idx2, t0.idx1))
					{
						float edgeLengthSq = (vertices[edge.idx0] - vertices[edge.idx1]).SizeSquared();
						/// Find distance between vertices that aren't on the edge,
						/// which is vert[0] from tri0 and vert[2] from tri1.
						float crossLengthSq = (vertices[triangles[tri0].idx0] - vertices[triangles[tri1].idx2]).SizeSquared();

						/// If that distance is shorter than edge length
						if (crossLengthSq < edgeLengthSq)
						{
							/// change tri0 to (k,i,l) and tri1 to (l,i,j)
							t0 = Triangle
							{
								triangles[tri0].idx2,
								triangles[tri0].idx0,
								triangles[tri1].idx2
							};
							t1 = Triangle
							{
								triangles[tri1].idx2,
								triangles[tri0].idx0,
								triangles[tri0].idx1
							};
							triangles[tri0] = t0;
							triangles[tri1] = t1;
						}
					}
				}
			}
		}

		bool EdgeHasVertex(Edge edge, int vertIdx)
		{
			return vertIdx == edge.idx0 || vertIdx == edge.idx1;
		}

		void ShiftTriangles(Edge edge, int tri0, int tri1)
		{
			Triangle t0 = triangles[tri0];
			while (t0.idx0 == edge.idx0 || t0.idx0 == edge.idx1)
			{
				int k = t0.idx0;
				t0.idx0 = t0.idx1;
				t0.idx1 = t0.idx2;
				t0.idx2 = k;
			}
			check(EdgeHasVertex(edge, t0.idx1));
			check(EdgeHasVertex(edge, t0.idx2));
			triangles[tri0] = t0;

			t0 = triangles[tri1];
			while (t0.idx2 == edge.idx0 || t0.idx2 == edge.idx1)
			{
				int k = t0.idx0;
				t0.idx0 = t0.idx1;
				t0.idx1 = t0.idx2;
				t0.idx2 = k;
			}
			check(EdgeHasVertex(edge, t0.idx0));
			check(EdgeHasVertex(edge, t0.idx1));
			triangles[tri1] = t0;
		}

		int FindTriangleWithEdge(Edge edge, int notTriangle)
		{
			for (int i = 0; i < triangles.Num(); ++i)
			{
				if (i != notTriangle)
				{
					Triangle tri = triangles[i];
					if (EdgesEqual(edge, tri.idx0, tri.idx1)
						|| EdgesEqual(edge, tri.idx1, tri.idx2)
						|| EdgesEqual(edge, tri.idx2, tri.idx0))
					{
						return i;
					}
				}
			}
			return -1;
		}

		bool EdgesEqual(Edge edge, int idx0, int idx1)
		{
			if (edge.idx0 == idx0 && edge.idx1 == idx1)
			{
				return true;
			}
			if (edge.idx1 == idx0 && edge.idx0 == idx1)
			{
				return true;
			}
			return false;
		}

		Edge ClosestEdge(IndexedBary bary)
		{
			Triangle tri = triangles[bary.triangle];
			Edge edge;
			edge.idx0 = tri.idx1;
			edge.idx1 = tri.idx2;
			float minWeight = bary.bary.weights[0];
			if (bary.bary.weights[1] < minWeight)
			{
				edge.idx0 = tri.idx0;
				edge.idx1 = tri.idx2;
				minWeight = bary.bary.weights[1];
			}
			if (bary.bary.weights[2] < minWeight)
			{
				edge.idx0 = tri.idx0;
				edge.idx1 = tri.idx1;
			}
			return edge;
		}

		IndexedBary FindTriangle(FVector pos);

		bool FindTriangleOrEdgeOrVertex(FVector pos, Interpolant& outBary)
		{
			if (PointInsideBounds(pos))
			{
				outBary = FindTriangle(pos).bary;

				if (IsInteriorTriangle(outBary))
				{
					return true;
				}
			}

			return FindClosestExteriorEdge(pos, outBary);
		}

		bool PointInsideBounds(FVector pos)
		{
			if (vertices.Num() < 4)
			{
				return false;
			}
			return pos.X >= vertices[1].X
				&& pos.X <= vertices[3].X
				&& pos.Y >= vertices[1].Y
				&& pos.Y <= vertices[3].Y;
		}

		bool IsInteriorTriangle(Interpolant bary)
		{
			/*if (bary == nullptr)
			{
				return false;
			}*/
			for (int i = 0; i < 3; ++i)
			{
				if (IsBoundary(bary.idx[i]))
				{
					return false;
				}
			}
			return true;
		}

		bool IsBoundary(int vertIdx)
		{
			return vertIdx < 4;
		}

		int HasExteriorEdge(Triangle tri)
		{
			int outVertIdx = -1;
			int numOutVerts = 0;
			if (IsBoundary(tri.idx0))
			{
				++numOutVerts;
				outVertIdx = 0;
			}
			if (IsBoundary(tri.idx1))
			{
				++numOutVerts;
				outVertIdx = 1;
			}
			if (IsBoundary(tri.idx2))
			{
				++numOutVerts;
				outVertIdx = 2;
			}
			if (numOutVerts == 1)
			{
				return outVertIdx;
			}
			return -1;
		}

		Edge ExtractEdge(Triangle tri, int outVertIdx)
		{
			Edge edge = Edge{ tri.idx0, tri.idx1 };
			switch (outVertIdx)
			{
			case 0:
			{
				edge.idx0 = tri.idx1;
				edge.idx1 = tri.idx2;
			}
			break;
			case 1:
			{
				edge.idx0 = tri.idx2;
				edge.idx1 = tri.idx0;
			}
			break;
			case 2:
				// already filled out correctly in initialization of edge.
				break;
			default:
				check(false); // Invalid vertex index, must be in [0..3).
				break;
			}
			if (edge.idx0 > edge.idx1)
			{
				int t = edge.idx0;
				edge.idx0 = edge.idx1;
				edge.idx1 = t;
			}
			return edge;
		}

		void FindExteriorEdges()
		{
			exteriorEdges.Empty();
			for (int iTri = 0; iTri < triangles.Num(); ++iTri)
			{
				Triangle tri = triangles[iTri];
				int outVertIdx = HasExteriorEdge(tri);
				if (outVertIdx >= 0)
				{
					exteriorEdges.Add(ExtractEdge(tri, outVertIdx));
				}
			}
			RemoveRedundantEdges(exteriorEdges);
		}

		void RemoveRedundantEdges(TArray<Edge> edges)
		{
			// Don't really need to check length, if it's empty or single edge nothing will happen.
			if (edges.Num() > 1)
			{
				edges.Sort([this](Edge e0, Edge e1)
				{
					// Sort by first index first.
					if (e0.idx0 < e1.idx0)
					{
						return false;
					}
					if (e0.idx0 > e1.idx0)
					{
						return true;
					}
					// First index equal, sort by second index.
					if (e0.idx1 < e1.idx1)
					{
						return false;
					}
					if (e0.idx1 > e1.idx1)
					{
						return true;
					}
					return false;
				});
				// Note i doesn't reach zero, because edges[i] is compared to edges[i-1].
				for (int i = edges.Num() - 1; i > 0; --i)
				{
					if (edges[i - 1].idx0 == edges[i].idx0 && edges[i - 1].idx1 == edges[i].idx1)
					{
						edges.RemoveAt(i);
					}
				}
			}
		}

		bool FindClosestExteriorEdge(FVector pos, Interpolant& outBary)
		{
			if (exteriorEdges.Num() == 0)
			{
				if (vertices.Num() == 5)
				{
					/// There is a single real vertex, so no exterior edges.
					/// That's okay, the single vertex wins all the weight.
					outBary.idx[0] = outBary.idx[1] = outBary.idx[2] = 4;
					outBary.weights[0] = 1.0f;
					outBary.weights[1] = outBary.weights[2] = 0.0f;
					return true;
				}
				return false;
			}
			int closestEdge = -1;
			float closestDistance = std::numeric_limits<float>::max();
			float closestParm = 0.0f;
			for (int i = 0; i < exteriorEdges.Num(); ++i)
			{
				PointOnEdge point = PositionOnEdge(exteriorEdges[i], pos);

				if (point.distanceSqr < closestDistance)
				{
					closestEdge = i;
					closestDistance = point.distanceSqr;
					closestParm = point.parm;
				}
			}
			check(closestEdge >= 0); // If there are any edges, there must be a closest one.
			Edge edge = exteriorEdges[closestEdge];
			outBary.idx[0] = edge.idx0;
			outBary.idx[1] = edge.idx1;
			outBary.idx[2] = 0;
			outBary.weights[0] = 1.0f - closestParm;
			outBary.weights[1] = closestParm;
			outBary.weights[2] = 0;

			return true;
		}

		PointOnEdge PositionOnEdge(Edge edge, FVector pos)
		{
			/// Project everything onto the horizontal plane.
			pos.Z = 0;
			FVector p0 = vertices[edge.idx0];
			p0.Z = 0;
			FVector p1 = vertices[edge.idx1];
			p1.Z = 0;

			FVector p0to1 = p1 - p0;
			float dist0to1Sqr = p0to1.SizeSquared();
			check(dist0to1Sqr > 0);
			float parm = FVector::DotProduct((pos - p0), p0to1) / dist0to1Sqr;
			parm = FMath::Clamp(parm, 0, 1);
			FVector pointOnEdge = p0 + parm * (p1 - p0);
			float distanceSqr = (pointOnEdge - pos).Size();

			return PointOnEdge { parm, distanceSqr };
		}

	};
}