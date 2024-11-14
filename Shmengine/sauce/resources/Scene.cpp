#include "Scene.hpp"

#include "core/Logging.hpp"
#include "core/FrameData.hpp"
#include "utility/Math.hpp"
#include "resources/Mesh.hpp"
#include "resources/Skybox.hpp"
#include "renderer/Camera.hpp"
#include "systems/RenderViewSystem.hpp"
#include "systems/LightSystem.hpp"
#include "systems/GeometrySystem.hpp"


bool32 scene_create(void* config, Scene* out_scene)
{

	*out_scene = {};
	return true;

}

bool32 scene_destroy(Scene* scene)
{
	return true;
}

bool32 scene_init(Scene* scene)
{
	return true;
}

bool32 scene_load(Scene* scene)
{
	return true;
}

bool32 scene_unload(Scene* scene)
{
	return true;
}

bool32 scene_update(Scene* scene)
{
	return true;
}

bool32 scene_build_render_packet(Scene* scene, Camera* cam, float32 aspect, FrameData* frame_data, Renderer::RenderPacket* out_packet)
{
	return true;
}

bool32 scene_add_directional_light(Scene* scene, LightSystem::DirectionalLight* light)
{
	return true;
}

bool32 scene_add_point_light(Scene* scene, LightSystem::DirectionalLight* light)
{
	return true;
}

bool32 scene_add_mesh(Scene* scene, Mesh* mesh)
{
	return true;
}

bool32 scene_add_skybox(Scene* scene, Skybox* skybox)
{
	return true;
}

bool32 scene_remove_directional_light(Scene* scene, LightSystem::DirectionalLight* light)
{
	return true;
}

bool32 scene_remove_point_light(Scene* scene, LightSystem::DirectionalLight* light)
{
	return true;
}

bool32 scene_remove_mesh(Scene* scene, Mesh* mesh)
{
	return true;
}

bool32 scene_remove_skybox(Scene* scene, Skybox* skybox)
{
	return true;
}
