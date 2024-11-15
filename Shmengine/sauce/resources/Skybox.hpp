#pragma once

#include "utility/MathTypes.hpp"
#include "systems/TextureSystem.hpp"

struct Geometry;

struct SkyboxConfig
{
	const char* cubemap_name;
};

struct Skybox
{
	const char* cubemap_name;
	TextureMap cubemap;
	Geometry* g;
	uint64 renderer_frame_number;
	uint32 instance_id;
};

SHMAPI bool32 skybox_create(SkyboxConfig* config, Skybox* out_skybox);
SHMAPI void skybox_destroy(Skybox* skybox);
SHMAPI bool32 skybox_init(Skybox* skybox);
SHMAPI bool32 skybox_load(Skybox* skybox);
SHMAPI bool32 skybox_unload(Skybox* skybox);
