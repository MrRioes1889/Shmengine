#pragma once

#include "Defines.hpp"
#include "containers/Sarray.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"

struct SkyboxConfig;
struct MeshConfig;
struct DirectionalLight;
struct PointLight;
struct MeshGeometryConfig;

namespace GeometrySystem { struct GeometryConfig; }

struct SceneSkyboxResourceData
{
	String name;
	String cubemap_name;
};

struct SceneMeshResourceData
{
	String name;
	String parent_name;
	String resource_name;
	Darray<MeshGeometryConfig> g_configs;
	Math::Transform transform;
};

struct SceneResourceData
{
	String name;
	String description;

	Math::Transform transform;

	uint32 max_meshes_count;
	uint32 max_p_lights_count;

	Sarray<SceneSkyboxResourceData> skyboxes;
	Sarray<DirectionalLight> dir_lights;
	Sarray<PointLight> point_lights;
	Sarray<SceneMeshResourceData> meshes;
};

namespace ResourceSystem
{
	bool32 scene_loader_load(const char* name, void* params, SceneResourceData* out_resource);
	void scene_loader_unload(SceneResourceData* resource);
}