#pragma once

#include "Defines.hpp"
#include "ResourceTypes.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"
#include "renderer/RendererTypes.hpp"

struct Box3D
{	
	UniqueId unique_id;
	ResourceState state;

	Math::Transform xform;
	Math::Vec4f color;

	GeometryData geometry;

	bool8 is_dirty;
};

SHMAPI bool8 box3D_init(Math::Vec3f size, Math::Vec4f color, Box3D* out_box);
SHMAPI bool8 box3D_destroy(Box3D* box);

SHMAPI bool8 box3D_update(Box3D* box);

SHMAPI void box3D_set_parent(Box3D* box, Math::Transform* parent);
SHMAPI void box3D_set_extents(Box3D* box, Math::Extents3D extents);
SHMAPI void box3D_set_color(Box3D* box, Math::Vec4f color);
