#pragma once

#include <Defines.hpp>
#include <core/Identifier.hpp>
#include <containers/Darray.hpp>
#include <utility/MathTypes.hpp>

#include <resources/ResourceTypes.hpp>
#include <resources/Skybox.hpp>
#include <resources/Mesh.hpp>
#include <resources/Terrain.hpp>

struct FrameData;
namespace Renderer 
{ 
	struct RenderPacket; 
	struct RenderViewRenderData;
}
struct RenderView;

struct DirectionalLight; 
struct PointLight;
struct Box3D;
namespace GeometrySystem { struct GeometryConfig; }
struct Camera;

struct SceneTerrainConfig
{
	const char* resource_name;
	TerrainConfig t_config;
	Math::Transform xform;
};

enum class SceneMeshType
{
	Resource,
	Cube
};

struct SceneMeshConfig
{
	struct CubeConfig
	{
		Math::Vec3f dim;
		Math::Vec2f tiling;
		const char* material_name;
	};

	SceneMeshType type;
	union
	{
		const char* resource_name;
		CubeConfig cube_config;
	};
	const char* name;
	const char* parent_name;
	Math::Transform transform;
};

struct SceneConfig
{
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

	SkyboxConfig* skybox_configs;
	DirectionalLight* dir_lights;
	PointLight* point_lights;
	SceneMeshConfig* mesh_configs;
	SceneTerrainConfig* terrain_configs;
};

struct Scene
{
	uint32 id;
	ResourceState state;

	bool8 enabled;

	String name;
	String description;

	Math::Transform transform;

	Skybox skybox;
	Darray<DirectionalLight> dir_lights;
	Darray<PointLight> p_lights;
	Darray<Box3D> p_light_boxes;
	Darray<Mesh> meshes;
	Darray<Terrain> terrains;
};

bool8 scene_init(SceneConfig* config, Scene* out_scene);
bool8 scene_init_from_resource(const char* resource_name, Scene* out_scene);
bool8 scene_destroy(Scene* scene);

bool8 scene_update(Scene* scene);

bool8 scene_add_directional_light(Scene* scene, DirectionalLight light);
bool8 scene_remove_directional_light(Scene* scene, uint32 index);

bool8 scene_add_point_light(Scene* scene, PointLight light);
bool8 scene_remove_point_light(Scene* scene, uint32 index);

bool8 scene_add_mesh(Scene* scene, SceneMeshConfig* config);
bool8 scene_remove_mesh(Scene* scene, const char* name);

bool8 scene_add_terrain(Scene* scene, SceneTerrainConfig* config);
bool8 scene_remove_terrain(Scene* scene, const char* name);

bool8 scene_add_skybox(Scene* scene, SkyboxConfig* config);
bool8 scene_remove_skybox(Scene* scene, const char* name);

Skybox* scene_get_skybox(Scene* scene, const char* name);
Mesh* scene_get_mesh(Scene* scene, const char* name);
Terrain* scene_get_terrain(Scene* scene, const char* name);
DirectionalLight* scene_get_dir_light(Scene* scene, uint32 index);
PointLight* scene_get_point_light(Scene* scene, uint32 index);

bool8 scene_draw(Scene* scene, const Math::Frustum* camera_frustum, FrameData* frame_data);

Math::Ray3DHitInfo scene_raycast(Scene* scene, Math::Ray3D ray);

