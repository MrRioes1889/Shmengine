#pragma once

#include "Defines.hpp"
#include "resources/ResourceTypes.hpp"
#include "renderer/RendererTypes.hpp"

namespace ShaderSystem
{

	struct Config
	{
		uint16 max_shader_count;
		uint16 max_uniform_count;
		uint16 max_global_textures;
		uint16 max_instance_textures;
	};

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config);
	void system_shutdown();

	SHMAPI bool32 create_shader(const Renderer::Renderpass* renderpass,const Renderer::ShaderConfig* config);

	SHMAPI uint32 get_id(const char* shader_name);

	SHMAPI Renderer::Shader* get_shader(uint32 shader_id);
	SHMAPI Renderer::Shader* get_shader(const char* shader_name);

	SHMAPI bool32 use_shader(uint32 shader_id);
	SHMAPI bool32 use_shader(const char* shader_name);

	SHMAPI uint16 get_uniform_index(Renderer::Shader* shader, const char* uniform_name);

	SHMAPI bool32 set_uniform(const char* uniform_name, const void* value);
	SHMAPI bool32 set_uniform(uint16 index, const void* value);
	SHMAPI bool32 set_sampler(const char* sampler_name, const void* value);
	SHMAPI bool32 set_sampler(uint16 index, const void* value);

	SHMAPI bool32 apply_global();
	SHMAPI bool32 apply_instance(bool32 needs_update);

	SHMAPI bool32 bind_instance(uint32 instance_id);

}