// Copyright (c) 2022 Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"

namespace WorldLockingTools
{
	// FTransform math to ensure poses are in the correct space for the FrozenWorld engine.
	class FFrozenWorldPoseExtensions
	{
	public:
		/*
		 * application of a transform on a position, defined such that:
		 * transform.position == FromGlobal(parent.transform) * localPosition
		 */
		static FVector Multiply(FTransform pose, FVector position)
		{
			return pose.GetLocation() + pose.GetRotation() * position;
		}

		/*
		 * chaining of transforms, defined such that
		 * V' = lhs * (rhs * V)
		 *	= (lhs.pos,lhs.rot) * (rhs.pos + rhs.rot * V)
		 *	= lhs.pos + lhs.rot * (rhs.pos + rhs.rot * V)
		 *	= lhs.pos + lhs.rot * rhs.pos + lhs.rot * rhs.rot * V
		 *	= (lhs.pos + lhs.rot * rhs.pos , lhs.rot * rhs.rot) * V
		 *	= (lhs * rhs) * V
		 */
		static FTransform Multiply(FTransform lhs, FTransform rhs)
		{
			return FTransform(lhs.GetRotation() * rhs.GetRotation(), lhs.GetLocation() + lhs.GetRotation() * rhs.GetLocation());
		}

		/*
		 * inverse of transform, defined such that
		 * 1 == inv(t) * t == t * inv(t)
		 *
		 *   inv(t) * t
		 * = (-inv(t.rot)*t.pos , inv(t.rot)) * (t.pos, t.rot)
		 * = (-inv(t.rot)*t.pos + inv(t.rot)  * t.pos , inv(t.rot) * t.rot)
		 * = 1
		 *
		 *   t * inv(t)
		 * = (t.pos, t.rot) * (-inv(t.rot)*t.pos , inv(t.rot))
		 * = (t.pos + t.rot * (-inv(t.rot)*t.pos) , t.rot * inv(t.rot))
		 * = 1
		 */
		static FTransform Inverse(FTransform pose)
		{
			FQuat invRotation = pose.GetRotation().Inverse();
			return FTransform(invRotation, -(invRotation * pose.GetLocation()));
		}
	};
}