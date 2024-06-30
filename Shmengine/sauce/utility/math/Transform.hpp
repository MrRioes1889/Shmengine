#pragma once

#include "../MathTypes.hpp"

namespace Math
{

	SHMAPI Transform transform_create();
	SHMAPI Transform transform_from_position(Vec3f position);
	SHMAPI Transform transform_from_rotation(Quat rotation);
	SHMAPI Transform transform_from_position_rotation(Vec3f position, Quat rotation);
	SHMAPI Transform transform_from_position_rotation_scale(Vec3f position, Quat rotation, Vec3f scale);

	SHMAPI void transform_translate(Transform& t, Vec3f translation);
	SHMAPI void transform_rotate(Transform& t, Quat rotation);
	SHMAPI void transform_scale(Transform& t, Vec3f scale);
	SHMAPI void transform_translate_rotate(Transform& t, Vec3f translation, Quat rotation);

	SHMAPI Mat4 transform_get_local(Transform& t);
	SHMAPI Mat4 transform_get_world(Transform& t);

}