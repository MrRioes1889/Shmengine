#pragma once

#include "Defines.hpp"
#include "ResourceTypes.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"
#include "renderer/RendererTypes.hpp"

enum class GizmoMode
{
	NONE,
	MOVE,
	ROTATE,
	SCALE
};

struct Gizmo3D
{
	UniqueId unique_id;
	ResourceState state;

	Math::Transform xform;
	GeometryData geometry;

	GizmoMode mode;

	bool8 is_dirty;
};

SHMAPI bool8 gizmo3D_init(Gizmo3D* out_gizmo);
SHMAPI bool8 gizmo3D_destroy(Gizmo3D* gizmo);
SHMAPI bool8 gizmo3D_load(Gizmo3D* gizmo);
SHMAPI bool8 gizmo3D_unload(Gizmo3D* gizmo);

SHMAPI bool8 gizmo3D_update(Gizmo3D* gizmo);

SHMAPI void gizmo3D_set_parent(Gizmo3D* gizmo, Math::Transform* parent);
SHMAPI void gizmo3D_set_mode(Gizmo3D* gizmo, GizmoMode mode);
