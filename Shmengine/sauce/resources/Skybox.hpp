#pragma once

#include "ResourceTypes.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/GeometrySystem.hpp"

struct GeometryData;
namespace Renderer 
{
	struct RenderViewInstanceData;
}

struct SkyboxConfig
{
	const char* name;
	const char* cubemap_name;
};

struct Skybox
{
	String name;
	ResourceState state;
	UniqueId unique_id;
	String cubemap_name;
	TextureMap cubemap;
	GeometryId geometry_id;
	uint32 shader_instance_id;
};

SHMAPI bool8 skybox_init(SkyboxConfig* config, Skybox* out_skybox);
SHMAPI bool8 skybox_destroy(Skybox* skybox);
