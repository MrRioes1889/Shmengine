#include "renderer/RendererFrontend.hpp"
#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "resources/loaders/TextureLoader.hpp"
#include "utility/math/Transform.hpp"

#include "systems/JobSystem.hpp"
#include "systems/ShaderSystem.hpp"

namespace Renderer
{
	extern SystemState* system_state;

	static bool8 _texture_init(TextureConfig* config, Texture* out_texture);
	static void _texture_destroy(Texture* texture);

	static void _texture_init_from_resource_job_success(void* params);
	static void _texture_init_from_resource_job_fail(void* params);
	static bool8 _texture_init_from_resource_job(uint32 thread_index, void* user_data);

	bool8 texture_init(TextureConfig* config, Texture* out_texture)
	{
		if (out_texture->state >= ResourceState::Initialized)
			return false;

		out_texture->state = ResourceState::Initializing;
		_texture_init(config, out_texture);
		out_texture->state = ResourceState::Initialized;

		return true;
	}

	struct TextureLoadParams
	{
		TextureType texture_type;
		char texture_name[Constants::max_texture_name_length];
		Texture* out_texture;
		Sarray<uint8>pixels;
	};

	bool8 texture_init_from_resource_async(const char* name, TextureType type, Texture* out_texture)
	{
		if (out_texture->state >= ResourceState::Initialized)
			return false;

		out_texture->state = ResourceState::Initializing;
		JobSystem::JobInfo job = JobSystem::job_create(_texture_init_from_resource_job, _texture_init_from_resource_job_success, _texture_init_from_resource_job_fail, sizeof(TextureLoadParams));
		TextureLoadParams* params = (TextureLoadParams*)job.user_data;
		CString::copy(name, params->texture_name, Constants::max_texture_name_length);
		params->out_texture = out_texture;
		params->texture_type = type;
		params->pixels = {};
		JobSystem::submit(job);

		return true;
	}

	bool8 texture_destroy(Texture* texture)
	{
		if (texture->state != ResourceState::Initialized)
			return false;

		texture->state = ResourceState::Destroying;
        _texture_destroy(texture);
		texture->state = ResourceState::Destroyed;
		return true;
	}

	static bool8 _texture_init(TextureConfig* config, Texture* out_texture)
	{
		CString::copy(config->name, out_texture->name, Constants::max_texture_name_length);
		out_texture->channel_count = config->channel_count;
		out_texture->width = config->width;
		out_texture->height = config->height;
		out_texture->type = config->type;
		out_texture->flags = config->flags;
		out_texture->flags &= ~TextureFlags::IsLoaded;
		if (config->pre_initialized_data)
		{
			out_texture->internal_data.init(config->pre_initialized_data_size, 0, AllocationTag::Texture, config->pre_initialized_data);
			return true;
		}

		goto_if(!system_state->module.texture_init(out_texture), fail);
		return true;

    fail:
        SHMERRORV("Failed to create texture '%s'.", out_texture->name);
        _texture_destroy(out_texture);
        return false;
	}

    static void _texture_destroy(Texture* texture)
    {
		system_state->module.texture_destroy(texture);
		texture->internal_data.free_data();
		texture->flags = 0;
		texture->name[0] = 0;
    }

	static void _texture_init_from_resource_job_success(void* params) 
	{
		TextureLoadParams* load_params = (TextureLoadParams*)params;

		Renderer::texture_write_data(load_params->out_texture, 0, load_params->pixels.capacity, load_params->pixels.data);

		load_params->pixels.free_data();
		load_params->out_texture->state = ResourceState::Initialized;
		SHMTRACEV("Successfully loaded texture '%s'.", load_params->out_texture->name);
	}

	static void _texture_init_from_resource_job_fail(void* params) 
	{
		TextureLoadParams* load_params = (TextureLoadParams*)params;
		load_params->pixels.free_data();
        load_params->out_texture->state = ResourceState::Destroyed;
		SHMERRORV("Failed to load texture '%s'.", load_params->out_texture->name);
	}

	static bool8 _texture_init_from_resource_job(uint32 thread_index, void* user_data) 
	{
		TextureLoadParams* load_params = (TextureLoadParams*)user_data;
		TextureConfig config = {};
		config.type = load_params->texture_type;
		config.flags = 0;
		config.name = load_params->texture_name;

		switch (config.type)
		{
		case TextureType::Plane:
		{
			TextureResourceData resource = {};
			if (!ResourceSystem::texture_loader_load(load_params->texture_name, true, &resource))
			{
				SHMERRORV("Failed to load image resources for texture '%s'", load_params->texture_name);
				return false;
			}

			config.channel_count = resource.channel_count;
			config.width = resource.width;
			config.height = resource.height;
			load_params->pixels.init((uint32)resource.pixels.size, 0);
			load_params->pixels.copy_memory(resource.pixels.data, (uint32)resource.pixels.size, 0);

			for (uint64 i = 0; i < load_params->pixels.capacity; i += config.channel_count)
			{
				uint8 a = load_params->pixels[i + 3];
				if (a < 255)
				{
					config.flags |= TextureFlags::HasTransparency;
					break;
				}
			}
			
			ResourceSystem::texture_loader_unload(&resource);
			break;
		}
		case TextureType::Cube:
		{
			char texture_names[6][Constants::max_texture_name_length];
			CString::safe_print_s(texture_names[0], Constants::max_texture_name_length, "%s_r", config.name);
			CString::safe_print_s(texture_names[1], Constants::max_texture_name_length, "%s_l", config.name);
			CString::safe_print_s(texture_names[2], Constants::max_texture_name_length, "%s_u", config.name);
			CString::safe_print_s(texture_names[3], Constants::max_texture_name_length, "%s_d", config.name);
			CString::safe_print_s(texture_names[4], Constants::max_texture_name_length, "%s_f", config.name);
			CString::safe_print_s(texture_names[5], Constants::max_texture_name_length, "%s_b", config.name);

			TextureResourceData resources[6] = {};
			bool8 resource_load_success = true;
			for (uint32 i = 0; i < 6; i++)
			{
				if (!ResourceSystem::texture_loader_load(texture_names[i], false, &resources[i]))
				{
					SHMERRORV("Failed to load image resources for texture '%s'", texture_names[i]);
					break;
				}
			}

			uint32 width = 0, height = 0;
			uint8 channel_count = 0;
			if (resource_load_success)
			{
				width = resources[0].width;
				height = resources[0].height;
				channel_count = resources[0].channel_count;
				for (uint32 i = 1; i < 6; i++)
				{
					if (resources[i].width != width || resources[i].height != height || resources[i].channel_count != channel_count)
					{
						SHMERRORV("Failed to load cube texture: Dimensions or channel counts do not match up!");
						resource_load_success = false;
					}
				}
			}

			if (resource_load_success)
			{
				config.width = width;
				config.height = height;
				config.channel_count = channel_count;

				uint32 pixel_buffer_size = 0;
				for (uint32 i = 0; i < 6; i++)
					pixel_buffer_size += (uint32)resources[i].pixels.size;

				load_params->pixels.init(pixel_buffer_size, 0);
				for (uint32 offset = 0, i = 0; i < 6; i++)
				{
					load_params->pixels.copy_memory(resources[i].pixels.data, (uint32)resources[i].pixels.size, offset);
					offset += (uint32)resources[i].pixels.size;
				}
			}

			for (uint32 i = 0; i < 6; i++)
				ResourceSystem::texture_loader_unload(&resources[i]);

			break;
		}
		default:
		{
			SHMASSERT_MSG(false, "Supplied unknown texture type.");
			return false;
		}
		}

		return _texture_init(&config, load_params->out_texture);
	}

	void texture_resize(Texture* texture, uint32 width, uint32 height)
	{
		system_state->module.texture_resize(texture, width, height);
	}

	bool8 texture_write_data(Texture* t, uint32 offset, uint32 size, const uint8* pixels)
	{
		return system_state->module.texture_write_data(t, offset, size, pixels);
	}

	bool8 texture_read_data(Texture* t, uint32 offset, uint32 size, void* out_memory)
	{
		return system_state->module.texture_read_data(t, offset, size, out_memory);
	}

	bool8 texture_read_pixel(Texture* t, uint32 x, uint32 y, uint32* out_rgba)
	{
		return system_state->module.texture_read_pixel(t, x, y, out_rgba);
	}
}
