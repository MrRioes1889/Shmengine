#pragma once

#include "systems/ResourceSystem.hpp"
#include "systems/ShaderSystem.hpp"

struct ShaderResourceData
{
	char name[max_shader_name_length];

	Darray<ShaderAttributeConfig> attributes;
	Darray<ShaderUniformConfig> uniforms;
	Darray<ShaderStageConfig> stages;

	bool8 depth_test;
	bool8 depth_write;

	Renderer::RenderCullMode cull_mode;
	Renderer::RenderTopologyTypeFlags::Value topologies;
};

namespace ResourceSystem
{
	bool32 shader_loader_load(const char* name, void* params, ShaderResourceData* out_resource);
	void shader_loader_unload(ShaderResourceData* resource);

	ShaderConfig shader_loader_get_config_from_resource(ShaderResourceData* resource);
}
