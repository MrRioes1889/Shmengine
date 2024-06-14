#include "TextureSystem.hpp"

#include "core/Logging.hpp"
#include "utility/String.hpp"
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
		Texture default_diffuse;

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
		system_state->registered_texture_table.init(config.max_texture_count, AllocationTag::UNKNOWN, hashtable_data);

		TextureReference invalid_ref;
		invalid_ref.auto_release = false;
		invalid_ref.handle = INVALID_OBJECT_ID;
		invalid_ref.reference_count = 0;
		system_state->registered_texture_table.floodfill(invalid_ref);

		for (uint32 i = 0; i < config.max_texture_count; i++)
		{
			system_state->registered_textures[i].id = INVALID_OBJECT_ID;
			system_state->registered_textures[i].generation = INVALID_OBJECT_ID;
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
				if (system_state->registered_textures[i].generation != INVALID_OBJECT_ID)
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
		
		if (String::equal_i(name, Config::default_diffuse_name))
		{
			SHMWARN("using regular acquire to recieve default texture. Should use 'get_default_texture()' instead!");
			return &system_state->default_diffuse;
		}

		TextureReference ref = system_state->registered_texture_table.get_value(name);
		ref.auto_release = auto_release;
		ref.reference_count++;
		if (ref.handle == INVALID_OBJECT_ID)
		{
			for (uint32 i = 0; i < system_state->config.max_texture_count; i++)
			{
				if (system_state->registered_textures[i].id == INVALID_OBJECT_ID)
				{
					ref.handle = i;
					break;
				}
			}

			if (ref.handle == INVALID_OBJECT_ID)
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

		if (String::equal_i(name, Config::default_diffuse_name))
			return;

		TextureReference ref = system_state->registered_texture_table.get_ref(name);
		if (ref.reference_count == 0)
		{
			SHMWARNV("Tried to release no existent texture: %s", name);
			return;
		}

		char name_copy[Texture::max_name_length] = {};
		String::copy(Texture::max_name_length, name_copy, name);

		ref.reference_count--;
		if (ref.reference_count == 0 && ref.auto_release)
		{
			Texture* t = &system_state->registered_textures[ref.handle];	

			destroy_texture(t);

			ref.handle = INVALID_OBJECT_ID;
			ref.auto_release = false;

			SHMTRACEV("Released Texture '%s'. Texture unloaded because reference count > 0 and auto_release enabled.", name_copy, ref.reference_count);
		}
		else
		{
			SHMTRACEV("Released Texture '%s'. ref count is now %i.", name_copy, ref.reference_count);
		}

		system_state->registered_texture_table.set_value(name_copy, ref);

	}

	Texture* get_default_texture()
	{
		return &system_state->default_diffuse;
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

		ResourceDataImage* img_resource_data = (ResourceDataImage*)img_resource.data;
		temp_texture.channel_count = img_resource_data->channel_count;
		temp_texture.width = img_resource_data->width;
		temp_texture.height = img_resource_data->height;

		uint32 current_generation = t->generation;
		t->generation = INVALID_OBJECT_ID;

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

		String::copy(Texture::max_name_length, temp_texture.name, texture_name);
		temp_texture.generation = INVALID_OBJECT_ID;
		temp_texture.has_transparency = has_transparency;

		Renderer::create_texture(pixels, &temp_texture);

		destroy_texture(t);
		(*t).move(temp_texture);

		if (current_generation == INVALID_OBJECT_ID)
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

		String::copy(Texture::max_name_length, system_state->default_diffuse.name, Config::default_diffuse_name);
		system_state->default_diffuse.width = tex_dim;
		system_state->default_diffuse.height = tex_dim;
		system_state->default_diffuse.channel_count = 4;
		system_state->default_diffuse.id = INVALID_OBJECT_ID;
		system_state->default_diffuse.generation = INVALID_OBJECT_ID;
		system_state->default_diffuse.has_transparency = false;

		Renderer::create_texture(pixels, &system_state->default_diffuse);
		system_state->default_diffuse.generation = INVALID_OBJECT_ID;

	}

	static void destroy_default_textures()
	{
		if (system_state)
		{
			destroy_texture(&system_state->default_diffuse);
		}
	}

	static void destroy_texture(Texture* t)
	{
		Renderer::destroy_texture(t);

		Memory::zero_memory(t, sizeof(t));
		t->id = INVALID_OBJECT_ID;
		t->generation = INVALID_OBJECT_ID;
	}
}