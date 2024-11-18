#pragma once

#include "utility/MathTypes.hpp"
#include "utility/String.hpp"
#include "systems/TextureSystem.hpp"

struct Geometry;

enum class SkyboxState
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

struct SkyboxConfig
{
	const char* name;
	const char* cubemap_name;
};

struct Skybox
{
	String name;
	String cubemap_name;
	TextureMap cubemap;
	Geometry* g;
	uint64 renderer_frame_number;
	uint32 instance_id;
	SkyboxState state;
};

SHMAPI bool32 skybox_create(SkyboxConfig* config, Skybox* out_skybox);
SHMAPI bool32 skybox_destroy(Skybox* skybox);
SHMAPI bool32 skybox_init(Skybox* skybox);
SHMAPI bool32 skybox_load(Skybox* skybox);
SHMAPI bool32 skybox_unload(Skybox* skybox);
