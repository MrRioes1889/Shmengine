#pragma once

#include "Defines.hpp"
#include "ResourceTypes.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"
#include "renderer/RendererTypes.hpp"

struct Line3D
{
	UniqueId unique_id;
	ResourceState state;

	Math::Transform xform;
	Math::Vec3f point0;
	Math::Vec3f point1;
	Math::Vec4f color;

	GeometryData geometry;

	bool8 is_dirty;
};

SHMAPI bool8 line3D_init(Math::Vec3f point0, Math::Vec3f point1, Math::Vec4f color, Line3D* out_line);
SHMAPI bool8 line3D_destroy(Line3D* line);

SHMAPI bool8 line3D_update(Line3D* line);

SHMAPI void line3D_set_parent(Line3D* line, Math::Transform* parent);
SHMAPI void line3D_set_points(Line3D* line, Math::Vec3f point0, Math::Vec3f point1);
SHMAPI void line3D_set_color(Line3D* line, Math::Vec4f color);