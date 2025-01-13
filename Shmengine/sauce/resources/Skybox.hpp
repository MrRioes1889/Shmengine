#pragma once

#include "utility/MathTypes.hpp"
#include "utility/String.hpp"
#include "systems/MaterialSystem.hpp"

struct GeometryData;
namespace Renderer 
{
	struct InstanceRenderData;
}

enum class SkyboxState
{
	UNINITIALIZED,
	DESTROYED,
	INITIALIZED,
	INITIALIZING,
	LOADING,
	LOADED,
	UNLOADING,
	UNLOADED
};

struct SkyboxConfig
{
	const char* name;
	const char* cubemap_name;
};

struct Skybox
{
	String name;
	SkyboxState state;
	UniqueId unique_id;
	String cubemap_name;
	TextureMap cubemap;
	GeometryData* geometry;
	uint32 shader_instance_id;
};

SHMAPI bool32 skybox_init(SkyboxConfig* config, Skybox* out_skybox);
SHMAPI bool32 skybox_destroy(Skybox* skybox);
SHMAPI bool32 skybox_load(Skybox* skybox);
SHMAPI bool32 skybox_unload(Skybox* skybox);

SHMAPI bool32 skybox_get_instance_render_data(void* in_skybox, Renderer::InstanceRenderData* out_data);
