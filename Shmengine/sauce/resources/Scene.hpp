#pragma once

#include "Defines.hpp"
#include "containers/Darray.hpp"
#include "utility/MathTypes.hpp"

#include "Skybox.hpp"
#include "Mesh.hpp"

struct FrameData;
namespace Renderer 
{ 
	struct RenderPacket; 
	struct GeometryRenderData;
}

struct DirectionalLight; 
struct PointLight;
namespace GeometrySystem { struct GeometryConfig; }
struct Mesh;
struct Skybox;
struct Camera;

enum class SceneState
{
	INVALID,
	DESTROYED,
	UNINITIALIZED,
	INITIALIZED,
	LOADING,
	LOADED,
	UNLOADING,
	UNLOADED
};

struct SceneConfig
{
	const char* name;
	const char* description;

	Math::Transform transform;

	uint32 dir_light_count;
	uint32 point_light_count;
	uint32 mesh_count;

	SkyboxConfig* skybox_config;
	DirectionalLight* dir_lights;
	PointLight* point_lights;
	MeshConfig* mesh_configs;
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

	Darray<Renderer::GeometryRenderData> geometries;
};

SHMAPI bool32 scene_create(SceneConfig* config, Scene* out_scene);
SHMAPI bool32 scene_destroy(Scene* scene);
SHMAPI bool32 scene_init(Scene* scene);
SHMAPI bool32 scene_load(Scene* scene);
SHMAPI bool32 scene_unload(Scene* scene);

SHMAPI bool32 scene_update(Scene* scene);

SHMAPI bool32 scene_build_render_packet(Scene* scene, const Math::Frustum* camera_frustum, FrameData* frame_data, Renderer::RenderPacket* packet);

SHMAPI bool32 scene_add_directional_light(Scene* scene, DirectionalLight light);
SHMAPI bool32 scene_remove_directional_light(Scene* scene, uint32 index);

SHMAPI bool32 scene_add_point_light(Scene* scene, PointLight light);
SHMAPI bool32 scene_remove_point_light(Scene* scene, uint32 index);

SHMAPI bool32 scene_add_mesh(Scene* scene, MeshConfig* config);
SHMAPI bool32 scene_remove_mesh(Scene* scene, const char* name);

SHMAPI bool32 scene_add_skybox(Scene* scene, SkyboxConfig* config);
SHMAPI bool32 scene_remove_skybox(Scene* scene, const char* name);

