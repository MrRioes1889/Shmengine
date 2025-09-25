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
		TextureType texture_type;
		char texture_name[Constants::max_texture_name_length];
		Texture* out_texture;
		Sarray<uint8>pixels;
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
	};	

	static SystemState* system_state = 0;
	
	static void _create_default_textures();
	static void _destroy_default_textures();

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

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
			Renderer::texture_destroy(texture);
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

		Renderer::texture_init_from_resource_async(name, type, texture);

        system_state->texture_ref_counters[id] = { 1, auto_destroy };
        return texture;
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
		
		TextureConfig config = {};
		config.name = name;
		config.width = width;
		config.height = height;
		config.channel_count = channel_count;
		config.type = TextureType::Plane;
		config.flags = has_transparency ? TextureFlags::HasTransparency : 0;
		goto_if(!Renderer::texture_init(&config, texture), fail);

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
			Renderer::texture_destroy(texture);
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

	Texture* get_default_specular_texture() 
	{
		return &system_state->default_specular;
	}

	Texture* get_default_normal_texture() 
	{
		return &system_state->default_normal;
	}

	static void _create_default_textures()
	{
		SHMTRACE("Creating default texture...");
		{
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

			TextureConfig config = {};
			config.name = SystemConfig::default_name;
			config.width = tex_dim;
			config.height = tex_dim;
			config.channel_count = 4;
			config.type = TextureType::Plane;
			config.flags = 0;

			Renderer::texture_init(&config, &system_state->default_texture);
			Renderer::texture_write_data(&system_state->default_texture, 0, sizeof(pixels), pixels);
		}

		{
			// Diffuse texture.
			SHMTRACE("Creating default diffuse texture...");
			uint8 diff_pixels[16 * 16 * 4];
			Memory::set_memory(diff_pixels, 0xFF, sizeof(diff_pixels));

			TextureConfig config = {};
			config.name = SystemConfig::default_diffuse_name;
			config.width = 16;
			config.height = 16;
			config.channel_count = 4;
			config.type = TextureType::Plane;
			config.flags = 0;

			Renderer::texture_init(&config, &system_state->default_diffuse);
			Renderer::texture_write_data(&system_state->default_diffuse, 0, sizeof(diff_pixels), diff_pixels);
		}

		{
			// Specular texture.
			SHMTRACE("Creating default specular texture...");
			uint8 spec_pixels[16 * 16 * 4];
			// Default spec map is black (no specular)
			Memory::zero_memory(spec_pixels, sizeof(spec_pixels));

			TextureConfig config = {};
			config.name = SystemConfig::default_specular_name;
			config.width = 16;
			config.height = 16;
			config.channel_count = 4;
			config.type = TextureType::Plane;
			config.flags = 0;

			Renderer::texture_init(&config, &system_state->default_specular);
			Renderer::texture_write_data(&system_state->default_specular, 0, sizeof(spec_pixels), spec_pixels);
		}

		{
			// Normal texture
			SHMTRACE("Creating default normal texture...");
			const uint32 channel_count = 4;
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

			TextureConfig config = {};
			config.name = SystemConfig::default_normal_name;
			config.width = 16;
			config.height = 16;
			config.channel_count = 4;
			config.type = TextureType::Plane;
			config.flags = 0;

			Renderer::texture_init(&config, &system_state->default_normal);
			Renderer::texture_write_data(&system_state->default_normal, 0, sizeof(normal_pixels), normal_pixels);
		}

	}

	static void _destroy_default_textures()
	{
		if (!system_state)
			return;

		Renderer::texture_destroy(&system_state->default_texture);
		Renderer::texture_destroy(&system_state->default_diffuse);
		Renderer::texture_destroy(&system_state->default_specular);
		Renderer::texture_destroy(&system_state->default_normal);
	}

}