#pragma once

#include <Defines.hpp>
#include <containers/Sarray.hpp>
#include <utility/MathTypes.hpp>
#include <utility/String.hpp>

struct SkyboxConfig;
struct MeshConfig;
struct DirectionalLight;
struct PointLight;
struct MeshGeometryResourceData;

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
	Darray<MeshGeometryResourceData> geometries;
	Math::Transform transform;
};

struct SceneTerrainResourceData
{
	String name;
	String resource_name;
	Math::Transform xform;
};

struct SceneResourceData
{
	String name;
	String description;

	Math::Transform transform;

	uint32 max_meshes_count;
	uint32 max_p_lights_count;
	uint32 max_terrains_count;

	Sarray<SceneSkyboxResourceData> skyboxes;
	Sarray<DirectionalLight> dir_lights;
	Sarray<PointLight> point_lights;
	Sarray<SceneMeshResourceData> meshes;
	Sarray<SceneTerrainResourceData> terrains;
};

namespace ResourceSystem
{
	bool8 scene_loader_load(const char* name, void* params, SceneResourceData* out_resource);
	void scene_loader_unload(SceneResourceData* resource);
}