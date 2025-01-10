#pragma once

#include "Defines.hpp"
#include "ResourceTypes.h"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"
#include "systems/GeometrySystem.hpp"

struct Box3D
{
	String name;
	Math::Transform xform;
	UniqueId unique_id;
	ResourceState state;

	Math::Vec4f color;

	GeometryData geometry;

	bool8 is_dirty;
};

SHMAPI bool32 box3D_init(const char* name, Math::Vec3f size, Math::Vec4f color, Box3D* out_box);
SHMAPI bool32 box3D_destroy(Box3D* box);
SHMAPI bool32 box3D_load(Box3D* box);
SHMAPI bool32 box3D_unload(Box3D* box);

SHMAPI bool32 box3D_update(Box3D* box);

SHMAPI void box3D_set_parent(Box3D* box, Math::Transform* parent);
SHMAPI void box3D_set_extents(Box3D* box, Math::Extents3D extents);
SHMAPI void box3D_set_color(Box3D* box, Math::Vec4f color);
