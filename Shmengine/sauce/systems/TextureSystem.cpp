#include "TextureSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "core/Memory.hpp"
#include "containers/Hashtable.hpp"

#include "renderer/RendererFrontend.hpp"

#include "systems/ResourceSystem.hpp"

namespace TextureSystem
{

	struct TextureReference
	{
		uint32 reference_count;
		uint32 handle;
		bool32 auto_release;
	};

	struct SystemState
	{
		Config config;
		Texture default_texture;
		Texture default_diffuse;
		Texture default_specular;
		Texture default_normal;

		Texture* registered_textures;
		Hashtable<TextureReference> registered_texture_table = {};
	};	

	static SystemState* system_state = 0;
	
	static bool32 load_texture(const char* texture_name, Texture* t);
	static void destroy_texture(Texture* t);

	static void create_default_textures();
	static void destroy_default_textures();

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config)
	{

		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		system_state->config = config;

		uint64 texture_array_size = sizeof(Texture) * config.max_texture_count;
		system_state->registered_textures = (Texture*)allocator_callback(texture_array_size);

		uint64 hashtable_data_size = sizeof(TextureReference) * config.max_texture_count;
		void* hashtable_data = allocator_callback(hashtable_data_size);
		system_state->registered_texture_table.init(config.max_texture_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

		TextureReference invalid_ref;
		invalid_ref.auto_release = false;
		invalid_ref.handle = INVALID_ID;
		invalid_ref.reference_count = 0;
		system_state->registered_texture_table.floodfill(invalid_ref);

		for (uint32 i = 0; i < config.max_texture_count; i++)
		{
			system_state->registered_textures[i].id = INVALID_ID;
			system_state->registered_textures[i].generation = INVALID_ID;
		}

		create_default_textures();

		return true;

	}

	void system_shutdown()
	{
		if (system_state)
		{
			for (uint32 i = 0; i < system_state->config.max_texture_count; i++)
			{
				if (system_state->registered_textures[i].generation != INVALID_ID)
					Renderer::destroy_texture(&system_state->registered_textures[i]);
			}

			destroy_default_textures();
			system_state = 0;
		}
	}

	Texture* acquire(const char* name, bool32 auto_release)
	{

		if (!system_state)
			return 0;
		
		if (CString::equal_i(name, Config::default_diffuse_name))
		{
			SHMWARN("using regular acquire to recieve default texture. Should use 'get_default_texture()' instead!");
			return &system_state->default_diffuse;
		}

		TextureReference ref = system_state->registered_texture_table.get_value(name);
		ref.auto_release = auto_release;
		ref.reference_count++;
		if (ref.handle == INVALID_ID)
		{
			for (uint32 i = 0; i < system_state->config.max_texture_count; i++)
			{
				if (system_state->registered_textures[i].id == INVALID_ID)
				{
					ref.handle = i;
					break;
				}
			}

			if (ref.handle == INVALID_ID)
			{
				SHMFATAL("Could not acquire new texture to texture system since no free slots are left!");
				return 0;
			}

			Texture* t = &system_state->registered_textures[ref.handle];
			if (!load_texture(name, t))
			{
				SHMERRORV("Failed to load texture: '%s'", name);
				return 0;
			}

			t->id = ref.handle;
			SHMTRACEV("Texture '%s' did not exist yet. Created and ref count is now %i.", name, ref.reference_count);
		}
		else
		{
			SHMTRACEV("Texture '%s' already existed. ref count is now %i.", name, ref.reference_count);		
		}

		system_state->registered_texture_table.set_value(name, ref);
		return &system_state->registered_textures[ref.handle];

	}

	void release(const char* name)
	{

		if (CString::equal_i(name, Config::default_name) ||
			CString::equal_i(name, Config::default_diffuse_name) || 
			CString::equal_i(name, Config::default_specular_name) || 
			CString::equal_i(name, Config::default_normal_name))
			return;

		TextureReference ref = system_state->registered_texture_table.get_ref(name);
		if (ref.reference_count == 0)
		{
			SHMWARNV("Tried to release no existent texture: %s", name);
			return;
		}

		char name_copy[Texture::max_name_length] = {};
		CString::copy(Texture::max_name_length, name_copy, name);

		ref.reference_count--;
		if (ref.reference_count == 0 && ref.auto_release)
		{
			Texture* t = &system_state->registered_textures[ref.handle];	

			destroy_texture(t);

			ref.handle = INVALID_ID;
			ref.auto_release = false;

			SHMTRACEV("Released Texture '%s'. Texture unloaded because reference count == 0 and auto_release enabled.", name_copy, ref.reference_count);
		}
		else
		{
			SHMTRACEV("Released Texture '%s'. ref count is now %i.", name_copy, ref.reference_count);
		}

		system_state->registered_texture_table.set_value(name_copy, ref);

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

	static bool32 load_texture(const char* texture_name, Texture* t)
	{

		Resource img_resource;
		if (!ResourceSystem::load(texture_name, ResourceType::IMAGE, &img_resource))
		{
			SHMERRORV("load_texture - Failed to load image resources for texture '%s'", texture_name);
			return false;
		}	

		Texture temp_texture;

		ImageConfig* img_resource_data = (ImageConfig*)img_resource.data;
		temp_texture.channel_count = img_resource_data->channel_count;
		temp_texture.width = img_resource_data->width;
		temp_texture.height = img_resource_data->height;

		uint32 current_generation = t->generation;
		t->generation = INVALID_ID;

		uint8* pixels = img_resource_data->pixels;
		uint64 size = temp_texture.width * temp_texture.height * temp_texture.channel_count;
		bool32 has_transparency = false;
		for (uint64 i = 0; i < size; i += img_resource_data->channel_count)
		{
			uint8 a = pixels[i + 3];
			if (a < 255)
			{
				has_transparency = true;
				break;
			}
		}

		CString::copy(Texture::max_name_length, temp_texture.name, texture_name);
		temp_texture.generation = INVALID_ID;
		temp_texture.has_transparency = has_transparency;

		Renderer::create_texture(pixels, &temp_texture);

		destroy_texture(t);
		(*t).move(temp_texture);

		if (current_generation == INVALID_ID)
			t->generation = 0;
		else
			t->generation = current_generation + 1;

		ResourceSystem::unload(&img_resource);

		return true;

	}

	static void create_default_textures()
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

		CString::copy(Texture::max_name_length, system_state->default_texture.name, Config::default_name);
		system_state->default_texture.width = tex_dim;
		system_state->default_texture.height = tex_dim;
		system_state->default_texture.channel_count = 4;
		system_state->default_texture.id = INVALID_ID;
		system_state->default_texture.generation = INVALID_ID;
		system_state->default_texture.has_transparency = false;

		Renderer::create_texture(pixels, &system_state->default_texture);
		system_state->default_texture.generation = INVALID_ID;

		// Diffuse texture.
		SHMTRACE("Creating default diffuse texture...");
		uint8 diff_pixels[16 * 16 * 4];
		Memory::set_memory(diff_pixels, 0xFF, sizeof(diff_pixels));
		CString::copy(Texture::max_name_length, system_state->default_diffuse.name, Config::default_diffuse_name);
		system_state->default_diffuse.width = 16;
		system_state->default_diffuse.height = 16;
		system_state->default_diffuse.channel_count = 4;
		system_state->default_diffuse.generation = INVALID_ID;
		system_state->default_diffuse.has_transparency = false;
		Renderer::create_texture(diff_pixels, &system_state->default_diffuse);
		// Manually set the texture generation to invalid since this is a default texture.
		system_state->default_diffuse.generation = INVALID_ID;

		// Specular texture.
		SHMTRACE("Creating default specular texture...");
		uint8 spec_pixels[16 * 16 * 4];
		// Default spec map is black (no specular)
		Memory::zero_memory(spec_pixels, sizeof(spec_pixels));
		CString::copy(Texture::max_name_length, system_state->default_specular.name, Config::default_specular_name);
		system_state->default_specular.width = 16;
		system_state->default_specular.height = 16;
		system_state->default_specular.channel_count = 4;
		system_state->default_specular.generation = INVALID_ID;
		system_state->default_specular.has_transparency = false;
		Renderer::create_texture(spec_pixels, &system_state->default_specular);
		// Manually set the texture generation to invalid since this is a default texture.
		system_state->default_specular.generation = INVALID_ID;

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

		CString::copy(Texture::max_name_length, system_state->default_normal.name, Config::default_normal_name);
		system_state->default_normal.width = 16;
		system_state->default_normal.height = 16;
		system_state->default_normal.channel_count = 4;
		system_state->default_normal.generation = INVALID_ID;
		system_state->default_normal.has_transparency = false;
		Renderer::create_texture(normal_pixels, &system_state->default_normal);
		// Manually set the texture generation to invalid since this is a default texture.
		system_state->default_normal.generation = INVALID_ID;

	}

	static void destroy_default_textures()
	{
		if (system_state)
		{
			destroy_texture(&system_state->default_texture);
			destroy_texture(&system_state->default_diffuse);
			destroy_texture(&system_state->default_specular);
			destroy_texture(&system_state->default_normal);
		}
	}

	static void destroy_texture(Texture* t)
	{
		Renderer::destroy_texture(t);

		Memory::zero_memory(t, sizeof(t));
		t->id = INVALID_ID;
		t->generation = INVALID_ID;
	}
}