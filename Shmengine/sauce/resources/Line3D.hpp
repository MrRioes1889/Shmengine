#pragma once

#include "Defines.hpp"
#include "ResourceTypes.h"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"
#include "systems/GeometrySystem.hpp"

struct Line3D
{
	String name;
	Math::Transform xform;
	UniqueId unique_id;
	ResourceState state;

	Math::Vec3f point0;
	Math::Vec3f point1;
	Math::Vec4f color;

	GeometryData geometry;

	bool8 is_dirty;
};

SHMAPI bool32 line3D_init(const char* name, Math::Vec3f point0, Math::Vec3f point1, Math::Vec4f color, Line3D* out_line);
SHMAPI bool32 line3D_destroy(Line3D* line);
SHMAPI bool32 line3D_load(Line3D* line);
SHMAPI bool32 line3D_unload(Line3D* line);

SHMAPI bool32 line3D_update(Line3D* line);

SHMAPI void line3D_set_parent(Line3D* line, Math::Transform* parent);
SHMAPI void line3D_set_points(Line3D* line, Math::Vec3f point0, Math::Vec3f point1);
SHMAPI void line3D_set_color(Line3D* line, Math::Vec4f color);