#pragma once

#include "Defines.hpp"
#include "Vec2.hpp"
#include "Vec4.hpp"

namespace Math
{

#pragma pack(push, 1)

	struct Rect2D
	{
		Vec2f pos;
		uint32 width, height;
	};

	struct Circle2D
	{
		Vec2f pos;
		float radius;
	};

#pragma pack(pop)

	typedef Vec4f Quat;

	//------- Quat functions

	SHMINLINE float32 quat_inner(Quat q1, Quat q2)
	{
		return inner_product(q1, q2);
	}

	SHMINLINE float32 quat_normal(Quat q)
	{
		return sqrt(quat_inner(q, q));
	}

	SHMINLINE Quat quat_normalize(Quat q)
	{
		float32 normal = quat_normal(q);
		return { q.x / normal, q.y / normal, q.z / normal, q.w / normal };
	}

	SHMINLINE Quat quat_conjugate(Quat q)
	{
		return { -q.x, -q.y, -q.z, q.w };
	}

	SHMINLINE Quat quat_inverse(Quat q)
	{
		return quat_normalize(quat_conjugate(q));
	}

	SHMINLINE Quat quat_mul(Quat q1, Quat q2)
	{
		Quat res;

		res.x =
			q1.x * q2.w +
			q1.y * q2.z -
			q1.z * q2.y +
			q1.w * q2.x;

		res.y =
			-q1.x * q2.w +
			q1.y * q2.z +
			q1.z * q2.y +
			q1.w * q2.x;

		res.z =
			q1.x * q2.w -
			q1.y * q2.z +
			q1.z * q2.y +
			q1.w * q2.x;

		res.w =
			-q1.x * q2.w -
			q1.y * q2.z -
			q1.z * q2.y +
			q1.w * q2.x;

		return res;
	}

	SHMINLINE Quat quat_from_axis_angle(Vec3f axis, float32 angle, bool32 normalize) {
		const float32 half_angle = 0.5f * angle;
		float32 s = sin(half_angle);
		float32 c = cos(half_angle);

		Quat q = { s * axis.x, s * axis.y, s * axis.z, c };
		if (normalize) {
			return quat_normalize(q);
		}
		return q;
	}

	SHMINLINE Quat quat_slerp(Quat q_0, Quat q_1, float32 percentage) {
		Quat out_quaternion;
		// Source: https://en.wikipedia.org/wiki/Slerp
		// Only unit quaternions are valid rotations.
		// Normalize to avoid undefined behavior.
		Quat v0 = quat_normalize(q_0);
		Quat v1 = quat_normalize(q_1);

		// Compute the cosine of the angle between the two vectors.
		float32 dot = quat_inner(v0, v1);

		// If the dot product is negative, slerp won't take
		// the shorter path. Note that v1 and -v1 are equivalent when
		// the negation is applied to all four components. Fix by
		// reversing one quaternion.
		if (dot < 0.0f) {
			v1.x = -v1.x;
			v1.y = -v1.y;
			v1.z = -v1.z;
			v1.w = -v1.w;
			dot = -dot;
		}

		const float32 DOT_THRESHOLD = 0.9995f;
		if (dot > DOT_THRESHOLD) {
			// If the inputs are too close for comfort, linearly interpolate
			// and normalize the result.
			out_quaternion = {
				v0.x + ((v1.x - v0.x) * percentage),
				v0.y + ((v1.y - v0.y) * percentage),
				v0.z + ((v1.z - v0.z) * percentage),
				v0.w + ((v1.w - v0.w) * percentage) };

			return quat_normalize(out_quaternion);
		}

		// Since dot is in range [0, DOT_THRESHOLD], acos is safe
		float32 theta_0 = acos(dot);          // theta_0 = angle between input vectors
		float32 theta = theta_0 * percentage;  // theta = angle between v0 and result
		float32 sin_theta = sin(theta);       // compute this value only once
		float32 sin_theta_0 = sin(theta_0);   // compute this value only once

		float32 s0 = cos(theta) - dot * sin_theta / sin_theta_0;  // == sin(theta_0 - theta) / sin(theta_0)
		float32 s1 = sin_theta / sin_theta_0;

		return {
			(v0.x * s0) + (v1.x * s1),
				(v0.y * s0) + (v1.y * s1),
				(v0.z * s0) + (v1.z * s1),
				(v0.w * s0) + (v1.w * s1)
		};
	}
}
