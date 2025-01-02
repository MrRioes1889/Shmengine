#include "Camera.hpp"
#include "utility/Math.hpp"
#include "utility/Utility.hpp"

void Camera::update_view()
{
	if (!is_dirty)
		return;

	Math::Mat4 rotation_mat = Math::mat_euler_xyz(euler_rotation.x, euler_rotation.y, euler_rotation.z);
	Math::Mat4 translation_mat = Math::mat_translation(position);

	view = Math::mat_inverse(Math::mat_mul(rotation_mat, translation_mat));
	is_dirty = false;
	return;
}

Camera::Camera()
{
	reset();
}

Camera::~Camera()
{
}

void Camera::reset()
{
	euler_rotation = VEC3_ZERO;
	position = VEC3_ZERO;
	is_dirty = false;
	view = MAT4_IDENTITY;
}

Math::Mat4& Camera::get_view()
{	
	update_view();
	return view;
}

Math::Vec3f Camera::get_forward()
{
	return Math::mat_forward(view);
}

Math::Vec3f Camera::get_backward()
{
	return Math::mat_backward(view);
}

Math::Vec3f Camera::get_left()
{
	return Math::mat_left(view);
}

Math::Vec3f Camera::get_right()
{
	return Math::mat_right(view);
}

Math::Vec3f Camera::get_up()
{
	return Math::mat_up(view);
}

void Camera::move_forward(float32 velocity)
{
	Math::Vec3f dir = get_forward();
	dir *= velocity;
	position += dir;
	is_dirty = true;
}

void Camera::move_backward(float32 velocity)
{
	Math::Vec3f dir = get_backward();
	dir *= velocity;
	position += dir;
	is_dirty = true;
}

void Camera::move_left(float32 velocity)
{
	Math::Vec3f dir = get_left();
	dir *= velocity;
	position += dir;
	is_dirty = true;
}

void Camera::move_right(float32 velocity)
{
	Math::Vec3f dir = get_right();
	dir *= velocity;
	position += dir;
	is_dirty = true;
}

void Camera::move_up(float32 velocity)
{
	Math::Vec3f dir = VEC3F_UP;
	dir *= velocity;
	position += dir;
	is_dirty = true;
}

void Camera::move_down(float32 velocity)
{
	Math::Vec3f dir = VEC3F_DOWN;
	dir *= velocity;
	position += dir;
	is_dirty = true;
}

void Camera::yaw(float32 velocity)
{
	euler_rotation.y += velocity;
	is_dirty = true;
}

void Camera::pitch(float32 velocity)
{
	euler_rotation.x += velocity;
	static const float32 limit = Math::deg_to_rad(89.0f);
	euler_rotation.x = clamp(euler_rotation.x, -limit, limit);
	is_dirty = true;
}


