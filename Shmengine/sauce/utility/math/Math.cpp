#include "../Math.hpp"
#include <math.h>
#include <stdlib.h>

#include "platform/Platform.hpp"

namespace Math
{

	static uint32 rand_seed = 0;

	int32 round_f_to_i(float32 x)
	{
		return (int32)roundf(x);
	}
	int64 round_f_to_i64(float32 x)
	{
		return (int64)roundf(x);
	}

	int32 round_f_to_i(float64 x)
	{
		return (int32)round(x);
	}
	int64 round_f_to_i64(float64 x)
	{
		return (int64)round(x);
	}

	int32 floor_f_to_i(float32 x)
	{
		return (int32)floorf(x);
	}

	int32 ceil_f_to_i(float32 x)
	{
		return (int32)ceilf(x);
	}

	float32 sqrt(float32 a)
	{
		float32 res = sqrtf(a);
		return res;
	}

	float32 sin(float32 x)
	{
		return sinf(x);
	}

	float32 cos(float32 x)
	{
		return cosf(x);
	}

	float32 tan(float32 x)
	{
		return tanf(x);
	}

	float32 acos(float32 x)
	{
		return acosf(x);
	}

	float32 abs(float32 x)
	{
		return fabsf(x);
	}

	static uint32 pcg_hash()
	{
		if (!rand_seed)
			rand_seed = (uint32)Platform::get_absolute_time();

		uint32 state = rand_seed * 747796405u + 2891336453u;
		uint32 word = ((state >> ((state >> 28u) + 4u)) ^ state);
		return (word >> 22u) ^ word;
	}

	uint32 random_uint32()
	{
		rand_seed = pcg_hash();
		return rand_seed;
	}

	int32 random_int32()
	{
		rand_seed = pcg_hash();
		return *(int32*)&rand_seed;
	}

	int32 random_int32_clamped(int32 min, int32 max)
	{
		rand_seed = pcg_hash();
		int32 res = (*(int32*)&rand_seed % (max - min + 1)) + min;
		return res;
	}

	float32 random_float32()
	{
		rand_seed = pcg_hash();
		return *(float32*)&rand_seed / RAND_MAX;
	}

	float32 random_float32_clamped(float32 min, float32 max)
	{
		rand_seed = pcg_hash();
		float32 res = (*(float32*)&rand_seed / RAND_MAX / (max - min)) + min;
		return res;
	}

	Plane3D plane_3d_create(Vec3f p1, Vec3f norm)
	{
		Plane3D p;
		p.normal = normalized(norm);
		p.distance = inner_product(p.normal, p1);
		return p;
	}

	Frustum frustum_create(Vec3f position, Vec3f forward, Vec3f right, Vec3f up, float32 aspect, float32 fov, float32 near, float32 far)
	{
		Frustum f;

		float32 half_v = far * tan(fov * 0.5f);
		float32 half_h = half_v * aspect;
		Vec3f forward_far = forward * far;

		// Top, bottom, right, left, far, near
		f.sides[0] = plane_3d_create((forward * near) + position, forward);
		f.sides[1] = plane_3d_create(position + forward_far, forward * -1.0f);
		f.sides[2] = plane_3d_create(position, cross_product(up, (forward_far + (right * half_h))));
		f.sides[3] = plane_3d_create(position, cross_product((forward_far - (right * half_h)), up));
		f.sides[4] = plane_3d_create(position, cross_product(right, (forward_far - (up * half_v))));
		f.sides[5] = plane_3d_create(position, cross_product((forward_far + (up * half_v)), right));

		return f;
	}

	float32 plane_signed_distance(Plane3D p, Vec3f position)
	{
		return inner_product(p.normal, position) - p.distance;
	}

	bool32 plane_intersects_sphere(Plane3D p, Vec3f center, float32 radius)
	{
		return plane_signed_distance(p, center) > -radius;
	}

	bool32 frustum_intersects_sphere(Frustum f, Vec3f center, float32 radius)
	{
		for (uint32 i = 0; i < 6; i++)
		{
			if (!plane_intersects_sphere(f.sides[i], center, radius))
				return false;
		}

		return true;
	}

	bool32 plane_intersects_aabb(Plane3D p, Vec3f center, Vec3f extents)
	{
		float32 r =
			extents.x * Math::abs(p.normal.x) +
			extents.y * Math::abs(p.normal.y) +
			extents.z * Math::abs(p.normal.z);

		return -r <= plane_signed_distance(p, center);
	}	

	bool32 frustum_intersects_aabb(Frustum f, Vec3f center, Vec3f extents)
	{
		for (uint32 i = 0; i < 6; i++)
		{
			if (!plane_intersects_aabb(f.sides[i], center, extents))
				return false;
		}

		return true;
	}

}

