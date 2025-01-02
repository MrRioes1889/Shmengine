#pragma once

#include "utility/MathTypes.hpp"
#include "utility/String.hpp"
#include "systems/MaterialSystem.hpp"

struct GeometryData;

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
	String cubemap_name;
	TextureMap cubemap;
	GeometryData* geometry;
	uint32 render_frame_number;
	uint32 shader_instance_id;
	SkyboxState state;
};

SHMAPI bool32 skybox_init(SkyboxConfig* config, Skybox* out_skybox);
SHMAPI bool32 skybox_destroy(Skybox* skybox);
SHMAPI bool32 skybox_load(Skybox* skybox);
SHMAPI bool32 skybox_unload(Skybox* skybox);

SHMAPI bool32 skybox_on_render(uint32 shader_id, LightingInfo lighting, Math::Mat4* model, void* skybox, uint32 frame_number);
