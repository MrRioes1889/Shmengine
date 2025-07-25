#include "../Math.hpp"
#include <math.h>
#include <stdlib.h>

#include "platform/Platform.hpp"

namespace Math
{

	static uint32 rand_seed = 0;

	int32 round_f_to_i(float32 x)
	{
		return (int32)(x + 0.5f);
	}
	int64 round_f_to_i64(float32 x)
	{
		return (int64)(x + 0.5f);
	}

	int32 round_f_to_i(float64 x)
	{
		return (int32)(x + 0.5);
	}
	int64 round_f_to_i64(float64 x)
	{
		return (int64)(x + 0.5);
	}

	int32 floor_f_to_i(float32 x)
	{
		return (int32)(x);
	}

	int32 ceil_f_to_i(float32 x)
	{
		return (int32)(x + 0.999999f);
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

	bool8 plane_intersects_sphere(Plane3D p, Vec3f center, float32 radius)
	{
		return plane_signed_distance(p, center) > -radius;
	}

	bool8 frustum_intersects_sphere(Frustum f, Vec3f center, float32 radius)
	{
		for (uint32 i = 0; i < 6; i++)
		{
			if (!plane_intersects_sphere(f.sides[i], center, radius))
				return false;
		}

		return true;
	}

	bool8 plane_intersects_aabb(Plane3D p, Vec3f center, Vec3f extents)
	{
		float32 r =
			extents.x * Math::abs(p.normal.x) +
			extents.y * Math::abs(p.normal.y) +
			extents.z * Math::abs(p.normal.z);

		return -r <= plane_signed_distance(p, center);
	}	

	bool8 frustum_intersects_aabb(Frustum f, Vec3f center, Vec3f extents)
	{
		for (uint32 i = 0; i < 6; i++)
		{
			if (!plane_intersects_aabb(f.sides[i], center, extents))
				return false;
		}

		return true;
	}

	Ray3D ray3D_create(Vec3f origin, Vec3f direction)
	{
		return { .origin = origin, .direction = direction };
	}

	Ray3D ray3D_create_from_screen(Vec2f screen_pos, Vec2f viewport_size, Vec3f origin, Mat4 view, Mat4 projection)
	{
		Ray3D ray = { 0 };
		ray.origin = origin;
		// Get normalized device coordinates (i.e. -1:1 range).
		Vec3f ray_ndc;
		ray_ndc.x = (2.0f * screen_pos.x) / viewport_size.x - 1.0f;
		ray_ndc.y = 1.0f - (2.0f * screen_pos.y) / viewport_size.y;
		ray_ndc.z = 1.0f;
		// Clip space
		Vec4f ray_clip = { ray_ndc.x, ray_ndc.y, -1.0f, 1.0f };
		// Eye/Camera
		Vec4f ray_eye = mat_mul_vec(mat_inverse(projection), ray_clip);
		// Unproject xy, change wz to "forward".
		ray_eye = { ray_eye.x, ray_eye.y, -1.0f, 0.0f };
		// Convert to world coordinates;
		Vec4f dir4 = mat_mul_vec(view, ray_eye);
		ray.direction = { dir4.x, dir4.y, dir4.z };
		normalize(ray.direction);
		return ray;
	}

	bool8 ray3D_cast_obb(Extents3D bb_extents, Mat4 bb_model, Ray3D ray, float32* out_dist)
	{
		// Intersection based on the Real-Time Rendering and Essential Mathematics for Games

		// The nearest "far" intersection (within the x, y and z plane pairs)
		float32 nearest_far_intersection = 0.0f;
		// The farthest "near" intersection (withing the x, y and z plane pairs)
		float32 farthest_near_intersection = 100000.0f;
		// Pick out the world position from the model matrix.
		Vec3f oriented_pos_world = { bb_model.data[12], bb_model.data[13], bb_model.data[14] };
		// Transform the extents - This will orient/scale them to the model matrix.
		bb_extents.min = mat_mul_vec(bb_model, bb_extents.min);
		bb_extents.max = mat_mul_vec(bb_model, bb_extents.max);
		// The distance between the world position and the ray's origin.
		Vec3f delta = oriented_pos_world - ray.origin;
		// Test for intersection with the other planes perpendicular to each axis.
		Vec3f x_axis = mat_right(bb_model);
		Vec3f y_axis = mat_up(bb_model);
		Vec3f z_axis = mat_backward(bb_model);
		Vec3f axes[3] = { x_axis, y_axis, z_axis };
		for (uint32 i = 0; i < 3; ++i) 
		{
			float32 e = inner_product(axes[i], delta);
			float32 f = inner_product(ray.direction, axes[i]);
			if (abs(f) > 0.0001f) 
			{
				// Store distances between the ray origin and the ray-plane intersections in t1, and t2.
				// Intersection with the "left" plane.
				float32 t1 = (e + bb_extents.min.e[i]) / f;
				// Intersection with the "right" plane.
				float32 t2 = (e + bb_extents.max.e[i]) / f;
				// Ensure that t1 is the nearest intersection, and swap if need be.
				if (t1 > t2) 
				{
					float32 temp = t1;
					t1 = t2;
					t2 = temp;
				}

				if (t2 < farthest_near_intersection)
					farthest_near_intersection = t2;

				if (t1 > nearest_far_intersection)
					nearest_far_intersection = t1;

				// If the "far" is closer than the "near", then we can say that there is no intersection.
				if (farthest_near_intersection < nearest_far_intersection)
					return false;

			}
			else 
			{
				// Edge case, where the ray is almost parallel to the planes, then they don't have any intersection.
				if (-e + bb_extents.min.e[i] > 0.0f || -e + bb_extents.max.e[i] < 0.0f)
					return false;
			}
		}
		// This basically prevents interections from within a bounding box if the ray originates there.
		if (nearest_far_intersection == 0.0f)
			return false;

		*out_dist = nearest_far_intersection;
		return true;
	}

}

