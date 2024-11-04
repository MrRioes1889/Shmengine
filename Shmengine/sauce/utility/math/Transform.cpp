#include "Transform.hpp"

#include "Vec3.hpp"
#include "Mat.hpp"

namespace Math
{

	static void update_local(Transform& t)
	{
		if (!t.is_dirty)
			return;

		t.local = mat_mul(quat_to_mat(t.rotation), mat_translation(t.position));
		t.local = mat_mul(mat_scale(t.scale), t.local);
		t.is_dirty = false;
	}

	Transform transform_create()
	{
		Transform t;
		t.position = VEC3_ZERO;
		t.rotation = QUAT_IDENTITY;
		t.scale = VEC3F_ONE;
		t.local = MAT4_IDENTITY;		
		t.parent = 0;
		t.is_dirty = true;
		return t;
	}

	Transform transform_from_position(Vec3f position)
	{
		Transform t;
		t.position = position;
		t.rotation = QUAT_IDENTITY;
		t.scale = VEC3F_ONE;
		t.local = MAT4_IDENTITY;
		t.parent = 0;
		t.is_dirty = true;
		return t;
	}

	Transform transform_from_rotation(Quat rotation)
	{
		Transform t;
		t.position = VEC3_ZERO;
		t.rotation = rotation;
		t.scale = VEC3F_ONE;
		t.local = MAT4_IDENTITY;
		t.parent = 0;
		t.is_dirty = true;
		return t;
	}

	Transform transform_from_position_rotation(Vec3f position, Quat rotation)
	{
		Transform t;
		t.position = position;
		t.rotation = rotation;
		t.scale = VEC3F_ONE;
		t.local = MAT4_IDENTITY;
		t.parent = 0;
		t.is_dirty = true;
		return t;
	}

	Transform transform_from_position_rotation_scale(Vec3f position, Quat rotation, Vec3f scale)
	{
		Transform t;
		t.position = position;
		t.rotation = rotation;
		t.scale = scale;
		t.local = MAT4_IDENTITY;
		t.parent = 0;
		t.is_dirty = true;
		return t;
	}

	void transform_set_position(Transform& t, Vec3f position)
	{
		t.position = position;
		t.is_dirty = true;
	}

	void transform_set_rotaion(Transform& t, Quat rotation)
	{
		t.rotation = rotation;
		t.is_dirty = true;
	}

	void transform_set_scale(Transform& t, Vec3f scale)
	{
		t.scale = scale;
		t.is_dirty = true;
	}

	void transform_translate(Transform& t, Vec3f translation)
	{
		t.position = t.position + translation;
		t.is_dirty = true;
	}

	void transform_rotate(Transform& t, Quat rotation)
	{
		t.rotation = quat_mul(t.rotation, rotation);
		t.is_dirty = true;
	}

	void transform_scale(Transform& t, Vec3f scale)
	{
		t.scale = vec_mul(t.scale, scale);
		t.is_dirty = true;
	}

	void transform_translate_rotate(Transform& t, Vec3f translation, Quat rotation)
	{
		t.position = t.position + translation;
		t.rotation = quat_mul(t.rotation, rotation);
		t.is_dirty = true;
	}

	Mat4 transform_get_local(Transform& t)
	{
		update_local(t);
		return t.local;
	}

	Mat4 transform_get_world(Transform& t)
	{
		update_local(t);
		if (t.parent)
		{
			Mat4 p = transform_get_world(*t.parent);
			return mat_mul(t.local, p);
		}
		return t.local;
	}

}


