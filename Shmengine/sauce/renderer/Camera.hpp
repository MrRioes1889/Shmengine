#pragma once

#include "utility/MathTypes.hpp"

struct SHMAPI Camera
{
public:
	Camera();
	~Camera();

	void reset();

	SHMINLINE Math::Vec3f get_position() { return position; }
	SHMINLINE void set_position(Math::Vec3f pos) { position = pos; is_dirty = true; }
	SHMINLINE Math::Vec3f get_rotation() { return euler_rotation; }
	SHMINLINE void set_rotation(Math::Vec3f rot) { euler_rotation = rot; is_dirty = true; }

	Math::Mat4& get_view();

	SHMINLINE Math::Vec3f get_forward();
	SHMINLINE Math::Vec3f get_backward();
	SHMINLINE Math::Vec3f get_left();
	SHMINLINE Math::Vec3f get_right();
	SHMINLINE Math::Vec3f get_up();

	void move_forward(float32 velocity);
	void move_backward(float32 velocity);
	void move_left(float32 velocity);
	void move_right(float32 velocity);
	void move_up(float32 velocity);
	void move_down(float32 velocity);

	void yaw(float32 velocity);
	void pitch(float32 velocity);

private:

	void update_view();

	Math::Vec3f position;
	Math::Vec3f euler_rotation;
	Math::Mat4 view;
	bool32 is_dirty;
};

