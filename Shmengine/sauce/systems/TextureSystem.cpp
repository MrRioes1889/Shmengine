#include "TextureSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "core/Memory.hpp"
#include "core/Mutex.hpp"
#include "containers/LinearStorage.hpp"
#include "platform/Platform.hpp"

#include "renderer/RendererFrontend.hpp"

#include "resources/loaders/TextureLoader.hpp"
#include "systems/JobSystem.hpp"


namespace TextureSystem
{

	struct TextureLoadParams
	{
		char resource_name[Constants::max_texture_name_length];
		Texture* out_texture;
		TextureResourceData resource;
	};

	struct ReferenceCounter
	{
		uint16 reference_count;
		bool8 auto_destroy;
	};

	struct SystemState
	{
		Texture default_texture;
		Texture default_diffuse;
		Texture default_specular;
		Texture default_normal;

		Sarray<ReferenceCounter> texture_ref_counters;
		LinearHashedStorage<Texture, TextureId, Constants::max_texture_name_length> texture_storage;

		// TODO: Build a Solution that does not block when texture data is uploaded to gpu.
		uint32 textures_loading_count;
	};	

	static SystemState* system_state = 0;
	
	static bool8 _create_texture_async(const char* texture_name, Texture* t);
	static bool8 _create_cube_textures(const char* name, const char texture_names[6][Constants::max_texture_name_length], Texture* t);
	static void _destroy_texture(Texture* t);

	static void _create_default_textures();
	static void _destroy_default_textures();

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->textures_loading_count = 0;

		uint64 ref_counter_array_size = system_state->texture_ref_counters.get_external_size_requirement(sys_config->max_texture_count);
		void* ref_counter_array_data = allocator_callback(allocator, ref_counter_array_size);
		system_state->texture_ref_counters.init(sys_config->max_texture_count, 0, AllocationTag::Array, ref_counter_array_data);

		uint64 storage_data_size = system_state->texture_storage.get_external_size_requirement(sys_config->max_texture_count);
		void* storage_data = allocator_callback(allocator, storage_data_size);
		system_state->texture_storage.init(sys_config->max_texture_count, AllocationTag::Dict, storage_data);

		_create_default_textures();

		return true;
	}

	void system_shutdown(void* state)
	{
		if (!system_state)
			return;
		
		auto iter = system_state->texture_storage.get_iterator();
		while (Texture* texture = iter.get_next())
		{
			TextureId id;
			system_state->texture_storage.release(texture->name, &id, &texture);
			_destroy_texture(texture);
		}
		system_state->texture_storage.destroy();

		_destroy_default_textures();
		system_state = 0;
	}

	Texture* acquire(const char* name, TextureType type, bool8 auto_destroy)
	{
        TextureId id;
        Texture* texture;

        system_state->texture_storage.acquire(name, &id, &texture);
		if (!id.is_valid())
		{
			SHMERROR("Failed to create texture: Out of memory!");
			return 0;
		}
		else if (!texture)
		{
            system_state->texture_ref_counters[id].reference_count++;
            return system_state->texture_storage.get_object(id);
		}

		switch (type)
		{
		case TextureType::Plane:
		{
			goto_if_log(!_create_texture_async(name, texture), fail, "Failed to create texture.");
			break;
		}
		case TextureType::Cube:
		{
			char texture_names[6][Constants::max_texture_name_length];
			CString::safe_print_s<const char*>(texture_names[0], Constants::max_texture_name_length, "%s_r", name);
			CString::safe_print_s<const char*>(texture_names[1], Constants::max_texture_name_length, "%s_l", name);
			CString::safe_print_s<const char*>(texture_names[2], Constants::max_texture_name_length, "%s_u", name);
			CString::safe_print_s<const char*>(texture_names[3], Constants::max_texture_name_length, "%s_d", name);
			CString::safe_print_s<const char*>(texture_names[4], Constants::max_texture_name_length, "%s_f", name);
			CString::safe_print_s<const char*>(texture_names[5], Constants::max_texture_name_length, "%s_b", name);

			goto_if_log(!_create_cube_textures(name, texture_names, texture), fail, "Failed to create cube texture");
			break;
		}
		default:
		{
			goto_if_log(true, fail, "Failed to create texture: Unknown type");
			break;
		}
		}

        system_state->texture_ref_counters[id] = { 1, auto_destroy };
        return texture;

    fail:
		SHMERRORV("Failed to create texture '%s'.", name);
		return 0;
	}

	Texture* acquire_writable(const char* name, uint32 width, uint32 height, uint8 channel_count, bool8 has_transparency)
	{
        TextureId id;
        Texture* texture;

        system_state->texture_storage.acquire(name, &id, &texture);
		if (!id.is_valid())
		{
			SHMERROR("Failed to create texture: Out of memory!");
			return 0;
		}
		else if (!texture)
		{
            system_state->texture_ref_counters[id].reference_count++;
            return system_state->texture_storage.get_object(id);
		}

		CString::copy(name, texture->name, Constants::max_texture_name_length);
		texture->width = width;
		texture->height = height;
		texture->channel_count = channel_count;
		texture->type = TextureType::Plane;
		texture->flags = 0;
		texture->flags |= has_transparency ? TextureFlags::HasTransparency : 0;
		goto_if(!Renderer::texture_init(texture), fail);

        system_state->texture_ref_counters[id] = { 1, true };
        return texture;

    fail:
		SHMERRORV("Failed to create texture '%s'.", name);
		return 0;

	}

	bool8 wrap_internal(const char* name, uint32 width, uint32 height, uint8 channel_count, bool8 has_transparency, bool8 is_writable, bool8 register_texture, void* internal_data, uint64 internal_data_size, Texture* out_texture)
	{
		TextureId id = TextureId::invalid_value;
		Texture* t = 0;

		if (register_texture)
		{
			system_state->texture_storage.acquire(name, &id, &t);
			if (!id.is_valid())
			{
				SHMERROR("Failed to wrap texture: Out of memory!");
				return false;
			}
			else if (!t)
			{
				SHMERRORV("Failed to wrap texture: Texture named '%s' already exists!", name);
				return false;
			}
		}
		else
		{
			t = out_texture;
			SHMTRACEV("wrap_internal created texture '%s', but not registering, resulting in an allocation. It is up to the caller to free this memory.", name);
		}

		CString::copy(name, t->name, Constants::max_texture_name_length);
		t->width = width;
		t->height = height;
		t->channel_count = channel_count;
		t->type = TextureType::Plane;
		t->flags = 0;
		t->flags |= has_transparency ? TextureFlags::HasTransparency : 0;
		t->flags |= TextureFlags::IsWrapped;
		t->internal_data.init(internal_data_size, 0, AllocationTag::Texture, internal_data);

		if (register_texture)
			system_state->texture_ref_counters[id] = { 1, true };

		return true;
	}

	bool8 set_internal(Texture* t, void* internal_data, uint64 internal_data_size)
	{	
		t->internal_data.init(internal_data_size, 0, AllocationTag::Texture, internal_data);
		return true;
	}

	bool8 resize(Texture* t, uint32 width, uint32 height, bool8 regenerate_internal_data)
	{
		t->width = width;
		t->height = height;

		if (!(t->flags & TextureFlags::IsWrapped) && regenerate_internal_data)
		{
			Renderer::texture_resize(t, width, height);
			return false;
		}
		return true;
	}

	void write_to_texture(Texture* t, uint32 offset, uint32 size, const uint8* pixels)
	{
		Renderer::texture_write_data(t, offset, size, pixels);
	}

	void release(const char* name)
	{
		TextureId id = system_state->texture_storage.get_id(name);
        if (!id.is_valid())
            return;

        ReferenceCounter* ref_counter = &system_state->texture_ref_counters[id];
        if (ref_counter->reference_count > 0)
			ref_counter->reference_count--;

        if (ref_counter->reference_count == 0 && ref_counter->auto_destroy)
		{
            Texture* texture;
            system_state->texture_storage.release(name, &id, &texture);
			_destroy_texture(texture);
		}
	}

	Texture* get_default_texture()
	{
		return &system_state->default_texture;
	}

	Texture* get_default_diffuse_texture()
	{
		return &system_state->default_diffuse;
	}

	Texture* get_default_specular_texture() {
		return &system_state->default_specular;
	}

	Texture* get_default_normal_texture() {
		return &system_state->default_normal;
	}

	void sleep_until_all_textures_loaded()
	{
		while (system_state->textures_loading_count)
			Platform::sleep(1);
	}

	static void texture_load_on_success(void* params)
	{
		TextureLoadParams* load_params = (TextureLoadParams*)params;
		uint32 image_size = load_params->out_texture->width * load_params->out_texture->height * load_params->out_texture->channel_count;
		TextureConfig config = ResourceSystem::texture_loader_get_config_from_resource(&load_params->resource);

		SHMTRACEV("Loading texture '%s'.", load_params->resource_name);
		Renderer::texture_init(load_params->out_texture);
		Renderer::texture_write_data(load_params->out_texture, 0, image_size, config.pixels);

		SHMTRACEV("Successfully loaded texture '%s'.", load_params->resource_name);
		ResourceSystem::texture_loader_unload(&load_params->resource);
		system_state->textures_loading_count--;

	}

	static void texture_load_on_failure(void* params)
	{
		TextureLoadParams* load_params = (TextureLoadParams*)params;
		SHMTRACEV("Failed to load texture '%s'.", load_params->resource_name);
		ResourceSystem::texture_loader_unload(&load_params->resource);
		system_state->textures_loading_count--;
	}

	static bool8 texture_load_job_start(uint32 thread_index, void* user_data)
	{
		TextureLoadParams* load_params = (TextureLoadParams*)user_data;
		uint8* pixels = 0;
		uint32 image_size = 0;
		TextureConfig config = {};

		goto_if_log(!ResourceSystem::texture_loader_load(load_params->resource_name, true, &load_params->resource), fail,
			"Failed to load image resources for texture '%s'", load_params->resource_name);

		config = ResourceSystem::texture_loader_get_config_from_resource(&load_params->resource);
		CString::copy(load_params->resource_name, load_params->out_texture->name, Constants::max_texture_name_length);
		load_params->out_texture->channel_count = config.channel_count;
		load_params->out_texture->width = config.width;
		load_params->out_texture->height = config.height;
		load_params->out_texture->type = TextureType::Plane;

		load_params->out_texture->flags &= ~TextureFlags::IsLoaded;

		pixels = config.pixels;
		image_size = load_params->out_texture->width * load_params->out_texture->height * load_params->out_texture->channel_count;
		config.has_transparency = false;
		for (uint64 i = 0; i < image_size; i += config.channel_count)
		{
			uint8 a = pixels[i + 3];
			if (a < 255)
			{
				config.has_transparency = true;
				break;
			}
		}

		CString::copy(load_params->resource_name, load_params->out_texture->name, Constants::max_texture_name_length);
		load_params->out_texture->flags = 0;
		load_params->out_texture->flags |= config.has_transparency ? TextureFlags::HasTransparency : 0;

		return true;
	fail:
		return false;
	}

	static bool8 _create_texture_async(const char* texture_name, Texture* t)
	{	
		system_state->textures_loading_count++;
		JobSystem::JobInfo job = JobSystem::job_create(texture_load_job_start, texture_load_on_success, texture_load_on_failure, sizeof(TextureLoadParams));
		TextureLoadParams* params = (TextureLoadParams*)job.user_data;
		uint32 name_length = CString::length(texture_name);
		CString::copy(texture_name, params->resource_name, Constants::max_texture_name_length);
		params->out_texture = t;
		params->resource = {};
		JobSystem::submit(job);
		return true;
	}

	static bool8 _create_cube_textures(const char* name, const char texture_names[6][Constants::max_texture_name_length], Texture* t)
	{
		Sarray<uint8> pixels = {};
		uint32 image_size = 0;

		for (uint32 i = 0; i < 6; i++)
		{
			TextureResourceData resource_data;
			if (!ResourceSystem::texture_loader_load(texture_names[i], false, &resource_data))
			{
				SHMERRORV("load_texture - Failed to load image resources for texture '%s'", texture_names[i]);
				return false;
			}
			TextureConfig config = ResourceSystem::texture_loader_get_config_from_resource(&resource_data);

			if (!pixels.data)
			{
				CString::copy(name, t->name, Constants::max_texture_name_length);
				t->channel_count = config.channel_count;
				t->width = config.width;
				t->height = config.height;
				t->type = TextureType::Cube;

				t->flags = 0;
				CString::copy(texture_names[i], t->name, Constants::max_texture_name_length);

				image_size = t->width * t->height * t->channel_count;
				pixels.init(image_size * 6, 0, AllocationTag::Texture);			
			}
			else
			{
				if (t->width != config.width || t->height != config.height || t->channel_count != config.channel_count)
				{
					SHMERROR("load_cube_textures - Cannot load cube textures since dimensions vary between textures!");
					pixels.free_data();
					return false;
				}
			}

			pixels.copy_memory(config.pixels, image_size, image_size * i);

			ResourceSystem::texture_loader_unload(&resource_data);

		}

		Renderer::texture_init(t);
		Renderer::texture_write_data(t, 0, pixels.capacity, pixels.data);
		pixels.free_data();
		return true;

	}

	static void _create_default_textures()
	{

		SHMTRACE("Creating default texture...");
		const uint32 tex_dim = 256;
		const uint32 channel_count = 4;
		const uint32 pixel_count = tex_dim * tex_dim;
		uint8 pixels[pixel_count * channel_count];
		Memory::set_memory(pixels, 0xFF, pixel_count * channel_count);

		for (uint32 row = 0; row < tex_dim; row++)
		{
			for (uint32 col = 0; col < tex_dim; col++)
			{
				uint32 pixel_index = (row * tex_dim) + col;
				uint32 channel_index = pixel_index * channel_count;
				if (row % 2)
				{
					if (col % 2)
					{
						pixels[channel_index + 0] = 0;
						pixels[channel_index + 1] = 0;
					}
				}
				else
				{
					if (!(col % 2))
					{
						pixels[channel_index + 0] = 0;
						pixels[channel_index + 1] = 0;
					}
				}

			}
		}

		CString::copy(SystemConfig::default_name, system_state->default_texture.name, Constants::max_texture_name_length);
		system_state->default_texture.width = tex_dim;
		system_state->default_texture.height = tex_dim;
		system_state->default_texture.channel_count = 4;
		system_state->default_texture.type = TextureType::Plane;
		system_state->default_texture.flags = 0;

		Renderer::texture_init(&system_state->default_texture);
		Renderer::texture_write_data(&system_state->default_texture, 0, sizeof(pixels), pixels);

		// Diffuse texture.
		SHMTRACE("Creating default diffuse texture...");
		uint8 diff_pixels[16 * 16 * 4];
		Memory::set_memory(diff_pixels, 0xFF, sizeof(diff_pixels));
		CString::copy(SystemConfig::default_diffuse_name, system_state->default_diffuse.name, Constants::max_texture_name_length);
		system_state->default_diffuse.width = 16;
		system_state->default_diffuse.height = 16;
		system_state->default_diffuse.channel_count = 4;
		system_state->default_diffuse.type = TextureType::Plane;
		system_state->default_diffuse.flags = 0;
		Renderer::texture_init(&system_state->default_diffuse);
		Renderer::texture_write_data(&system_state->default_diffuse, 0, sizeof(diff_pixels), diff_pixels);

		// Specular texture.
		SHMTRACE("Creating default specular texture...");
		uint8 spec_pixels[16 * 16 * 4];
		// Default spec map is black (no specular)
		Memory::zero_memory(spec_pixels, sizeof(spec_pixels));
		CString::copy(SystemConfig::default_specular_name, system_state->default_specular.name, Constants::max_texture_name_length);
		system_state->default_specular.width = 16;
		system_state->default_specular.height = 16;
		system_state->default_specular.channel_count = 4;
		system_state->default_specular.type = TextureType::Plane;
		system_state->default_specular.flags = 0;
		Renderer::texture_init(&system_state->default_specular);
		Renderer::texture_write_data(&system_state->default_specular, 0, sizeof(spec_pixels), spec_pixels);

		// Normal texture
		SHMTRACE("Creating default normal texture...");
		uint8 normal_pixels[16 * 16 * 4];  // w * h * channels
		Memory::zero_memory(normal_pixels, sizeof(normal_pixels));

		// Each pixel.
		for (uint32 row = 0; row < 16; ++row) {
			for (uint32 col = 0; col < 16; ++col) {
				uint32 index = (row * 16) + col;
				uint32 index_bpp = index * channel_count;
				// Set blue, z-axis by default and alpha.
				normal_pixels[index_bpp + 0] = 128;
				normal_pixels[index_bpp + 1] = 128;
				normal_pixels[index_bpp + 2] = 255;
				normal_pixels[index_bpp + 3] = 255;
			}
		}

		CString::copy(SystemConfig::default_normal_name, system_state->default_normal.name, Constants::max_texture_name_length);
		system_state->default_normal.width = 16;
		system_state->default_normal.height = 16;
		system_state->default_normal.channel_count = 4;
		system_state->default_normal.type = TextureType::Plane;
		system_state->default_normal.flags = 0;
		Renderer::texture_init(&system_state->default_normal);
		Renderer::texture_write_data(&system_state->default_normal, 0, sizeof(normal_pixels), normal_pixels);

	}

	static void _destroy_default_textures()
	{
		if (system_state)
		{
			_destroy_texture(&system_state->default_texture);
			_destroy_texture(&system_state->default_diffuse);
			_destroy_texture(&system_state->default_specular);
			_destroy_texture(&system_state->default_normal);
		}
	}

	static void _destroy_texture(Texture* t)
	{
		Renderer::texture_destroy(t);

		t->flags = 0;
		t->name[0] = 0;
	}
}