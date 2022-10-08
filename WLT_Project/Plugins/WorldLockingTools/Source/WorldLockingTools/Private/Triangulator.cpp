// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#include "Triangulator.h"

namespace WorldLockingTools
{
	IndexedBary FTriangulator::FindTriangle(FVector pos)
	{
		IndexedBary bary;
		bary.bary = Interpolant();
		for (int i = 0; i < triangles.Num(); ++i)
		{
			Triangle tri = triangles[i];
			FVector p0 = vertices[tri.idx0];
			p0.Z = 0;
			FVector p1 = vertices[tri.idx1];
			p1.Z = 0;
			FVector p2 = vertices[tri.idx2];
			p2.Z = 0;
			/// Note area will be negative, because this is xz, not xy, but that will cancel with the negative cross products below.
			float area = FVector::CrossProduct(p2 - p1, p0 - p1).Z;
			if (area >= 0)
			{
				area = 0;
			}
			check(area < 0); // Degenerate triangle in Find

			FVector ps(pos.X, pos.Y, 0.0f);

			bary.bary.weights[0] = FVector::CrossProduct(p2 - p1, ps - p1).Z / area;
			bary.bary.weights[1] = FVector::CrossProduct(p0 - p2, ps - p2).Z / area;
			bary.bary.weights[2] = FVector::CrossProduct(p1 - p0, ps - p0).Z / area;

			if (bary.bary.IsInterior())
			{
				bary.triangle = i;
				bary.bary.idx[0] = tri.idx0;
				bary.bary.idx[1] = tri.idx1;
				bary.bary.idx[2] = tri.idx2;

				return bary;
			}

		}
		check(false); // No triangle exists where this is an interior position.
		return bary;
	}
}