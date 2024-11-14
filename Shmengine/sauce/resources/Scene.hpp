#pragma once

#include "Defines.hpp"
#include "containers/Darray.hpp"
#include "utility/MathTypes.hpp"


struct FrameData;
namespace Renderer { struct RenderPacket; }
namespace LightSystem 
{ 
	struct DirectionalLight; 
	struct PointLight;
}
namespace GeometrySystem { struct GeometryConfig; }
struct Mesh;
struct Skybox;
struct Camera;

enum class SceneState
{
	UNINITIALIZED,
	INITIALIZED,
	LOADING,
	LOADED,
	UNLOADING,
	UNLOADED
};

struct PendingMesh
{
	Mesh* mesh;
	const char* mesh_resource_name;
	GeometrySystem::GeometryConfig** g_configs;
	uint32 g_config_count;
};

struct Scene
{
	uint32 id;
	SceneState state;

	bool8 enabled;

	Math::Transform transform;
	Skybox* skybox;
	LightSystem::DirectionalLight* dir_light;
	Darray<LightSystem::PointLight*> p_lights;
	Darray<Mesh*> meshes;

	Darray<PendingMesh> pending_meshes;
};

SHMAPI bool32 scene_create(void* config, Scene* out_scene);
SHMAPI bool32 scene_destroy(Scene* scene);
SHMAPI bool32 scene_init(Scene* scene);
SHMAPI bool32 scene_load(Scene* scene);
SHMAPI bool32 scene_unload(Scene* scene);

SHMAPI bool32 scene_update(Scene* scene);

SHMAPI bool32 scene_build_render_packet(Scene* scene, Camera* cam, float32 aspect, FrameData* frame_data, Renderer::RenderPacket* out_packet);

SHMAPI bool32 scene_add_directional_light(Scene* scene, LightSystem::DirectionalLight* light);
SHMAPI bool32 scene_add_point_light(Scene* scene, LightSystem::DirectionalLight* light);
SHMAPI bool32 scene_add_mesh(Scene* scene, Mesh* mesh);
SHMAPI bool32 scene_add_skybox(Scene* scene, Skybox* skybox);

SHMAPI bool32 scene_remove_directional_light(Scene* scene, LightSystem::DirectionalLight* light);
SHMAPI bool32 scene_remove_point_light(Scene* scene, LightSystem::DirectionalLight* light);
SHMAPI bool32 scene_remove_mesh(Scene* scene, Mesh* mesh);
SHMAPI bool32 scene_remove_skybox(Scene* scene, Skybox* skybox);

