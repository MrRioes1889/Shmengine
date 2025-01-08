#pragma once

#include "Defines.hpp"
#include "containers/Darray.hpp"
#include "utility/MathTypes.hpp"

#include "resources/Skybox.hpp"
#include "resources/Mesh.hpp"
#include "resources/Terrain.hpp"

struct FrameData;
namespace Renderer 
{ 
	struct RenderPacket; 
	struct ObjectRenderData;
}

struct DirectionalLight; 
struct PointLight;
namespace GeometrySystem { struct GeometryConfig; }
struct Camera;

enum class SceneState
{
	UNINITIALIZED,
	DESTROYED,
	INITIALIZING,
	INITIALIZED,
	LOADING,
	LOADED,
	UNLOADING,
	UNLOADED
};

struct SceneTerrainConfig
{
	const char* resource_name;
	TerrainConfig t_config;
	Math::Transform xform;
};

struct SceneMeshConfig
{
	const char* resource_name;
	const char* parent_name;
	MeshConfig m_config;

	Math::Transform transform;
};

struct SceneConfig
{
	struct TerrainResource
	{
		const char* name;
		const char* resource_name;
	};

	const char* name;
	const char* description;

	Math::Transform transform;

	uint32 max_p_lights_count;
	uint32 max_meshes_count;
	uint32 max_terrains_count;

	uint32 skybox_configs_count;
	uint32 dir_light_count;
	uint32 point_light_count;
	uint32 mesh_configs_count;
	uint32 terrain_configs_count;
	uint32 terrain_resources_count;

	SkyboxConfig* skybox_configs;
	DirectionalLight* dir_lights;
	PointLight* point_lights;
	SceneMeshConfig* mesh_configs;
	SceneTerrainConfig* terrain_configs;
};

struct Scene
{
	uint32 id;
	SceneState state;

	bool8 enabled;

	String name;
	String description;

	Math::Transform transform;

	Skybox skybox;
	Darray<DirectionalLight> dir_lights;
	Darray<PointLight> p_lights;
	Darray<Mesh> meshes;
	Darray<Terrain> terrains;
};

SHMAPI bool32 scene_init(SceneConfig* config, Scene* out_scene);
SHMAPI bool32 scene_init_from_resource(const char* resource_name, Scene* out_scene);
SHMAPI bool32 scene_destroy(Scene* scene);
SHMAPI bool32 scene_load(Scene* scene);
SHMAPI bool32 scene_unload(Scene* scene);

SHMAPI bool32 scene_update(Scene* scene);

SHMAPI bool32 scene_add_directional_light(Scene* scene, DirectionalLight light);
SHMAPI bool32 scene_remove_directional_light(Scene* scene, uint32 index);

SHMAPI bool32 scene_add_point_light(Scene* scene, PointLight light);
SHMAPI bool32 scene_remove_point_light(Scene* scene, uint32 index);

SHMAPI bool32 scene_add_mesh(Scene* scene, SceneMeshConfig* config);
SHMAPI bool32 scene_remove_mesh(Scene* scene, const char* name);

SHMAPI bool32 scene_add_terrain(Scene* scene, SceneTerrainConfig* config);
SHMAPI bool32 scene_remove_terrain(Scene* scene, const char* name);

SHMAPI bool32 scene_add_skybox(Scene* scene, SkyboxConfig* config);
SHMAPI bool32 scene_remove_skybox(Scene* scene, const char* name);

SHMAPI Skybox* scene_get_skybox(Scene* scene, const char* name);
SHMAPI Mesh* scene_get_mesh(Scene* scene, const char* name);
SHMAPI Terrain* scene_get_terrain(Scene* scene, const char* name);
SHMAPI DirectionalLight* scene_get_dir_light(Scene* scene, uint32 index);
SHMAPI PointLight* scene_get_point_light(Scene* scene, uint32 index);

