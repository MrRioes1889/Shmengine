#include "TextureSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "core/Memory.hpp"
#include "containers/Hashtable.hpp"

#include "renderer/RendererFrontend.hpp"

#include "systems/ResourceSystem.hpp"
#include "systems/JobSystem.hpp"

namespace TextureSystem
{

	struct TextureLoadParams
	{
		char* resource_name;
		Texture* out_texture;
		Texture temp_texture;
		uint32 current_generation;
		Resource image_resource;
	};

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

	static bool32 add_texture_reference(const char* name, TextureType type, bool32 auto_release, bool32 skip_load, uint32* out_texture_id);
	static bool32 remove_texture_reference(const char* name, uint32* out_texture_id);

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
					Renderer::texture_destroy(&system_state->registered_textures[i]);
			}

			destroy_default_textures();
			system_state = 0;
		}
	}

	Texture* acquire(const char* name, bool32 auto_release)
	{
		
		if (CString::equal_i(name, Config::default_diffuse_name))
		{
			SHMWARN("acquire - using regular acquire to recieve default texture. Should use 'get_default_texture()' instead!");
			return &system_state->default_diffuse;
		}

		uint32 id = INVALID_ID;
		if (!add_texture_reference(name, TextureType::TYPE_2D, auto_release, false, &id))
		{
			SHMERRORV("acquire - failed to obtain new id for texture: '%s'.", name);
			return 0;
		}

		return &system_state->registered_textures[id];

	}

	Texture* acquire_cube(const char* name, bool32 auto_release)
	{

		if (CString::equal_i(name, Config::default_diffuse_name))
		{
			SHMWARN("acquire - using regular acquire to recieve default texture. Should use 'get_default_texture()' instead!");
			return &system_state->default_diffuse;
		}

		uint32 id = INVALID_ID;
		if (!add_texture_reference(name, TextureType::TYPE_CUBE, auto_release, false, &id))
		{
			SHMERRORV("acquire - failed to obtain new id for texture: '%s'.", name);
			return 0;
		}

		return &system_state->registered_textures[id];

	}

	Texture* acquire_writable(const char* name, uint32 width, uint32 height, uint32 channel_count, bool32 has_transparency)
	{

		uint32 id = INVALID_ID;
		if (!add_texture_reference(name, TextureType::TYPE_2D, false, true, &id))
		{
			SHMERRORV("acquire_writable - failed to obtain new id for texture: '%s'.", name);
			return 0;
		}

		Texture* t = &system_state->registered_textures[id];
		t->id = id;
		CString::copy(Texture::max_name_length, t->name, name);
		t->width = width;
		t->height = height;
		t->channel_count = channel_count;
		t->generation = INVALID_ID;
		t->type = TextureType::TYPE_2D;
		t->flags = 0;
		t->flags |= has_transparency ? TextureFlags::HAS_TRANSPARENCY : 0;
		t->flags |= TextureFlags::IS_WRITABLE;
		Renderer::texture_create_writable(t);

		return t;

	}

	Texture* wrap_internal(const char* name, uint32 width, uint32 height, uint32 channel_count, bool32 has_transparency, bool32 is_writable, bool32 register_texture, void* internal_data, uint64 internal_data_size)
	{

		uint32 id = INVALID_ID;
		Texture* t = 0;

		if (register_texture)
		{

			if (!add_texture_reference(name, TextureType::TYPE_2D, false, true, &id))
			{
				SHMERRORV("wrap_internal - failed to obtain new id for texture: '%s'.", name);
				return 0;
			}

			t = &system_state->registered_textures[id];
		}
		else
		{
			// TODO: Think about whether this is necessary
			t = (Texture*)Memory::allocate(sizeof(Texture), AllocationTag::TEXTURE);
			SHMTRACEV("wrap_internal created texture '%s', but not registering, resulting in an allocation. It is up to the caller to free this memory.", name);
		}

		t->id = id;
		CString::copy(Texture::max_name_length, t->name, name);
		t->width = width;
		t->height = height;
		t->channel_count = channel_count;
		t->generation = INVALID_ID;
		t->type = TextureType::TYPE_2D;
		t->flags = 0;
		t->flags |= has_transparency ? TextureFlags::HAS_TRANSPARENCY : 0;
		t->flags |= TextureFlags::IS_WRITABLE;
		t->flags |= TextureFlags::IS_WRAPPED;
		t->internal_data.init(internal_data_size, 0, AllocationTag::TEXTURE, internal_data);
		return t;

	}

	bool32 set_internal(Texture* t, void* internal_data, uint64 internal_data_size)
	{	
		t->internal_data.init(internal_data_size, 0, AllocationTag::TEXTURE, internal_data);
		t->generation++;
		return true;
	}

	bool32 resize(Texture* t, uint32 width, uint32 height, bool32 regenerate_internal_data)
	{

		if (!(t->flags & TextureFlags::IS_WRITABLE))
		{
			SHMWARNV("resize - Non-writable texture '%s' cannot be resized!", t->name);
			return false;
		}

		t->width = width;
		t->height = height;

		if (!(t->flags & TextureFlags::IS_WRAPPED) && regenerate_internal_data)
		{
			Renderer::texture_resize(t, width, height);
			return false;
		}
		t->generation++;
		return true;

	}

	void write_to_texture(Texture* t, uint32 offset, uint32 size, const uint8* pixels)
	{
		Renderer::texture_write_data(t, offset, size, pixels);
	}

	void release(const char* name)
	{

		if (CString::equal_i(name, Config::default_name) ||
			CString::equal_i(name, Config::default_diffuse_name) || 
			CString::equal_i(name, Config::default_specular_name) || 
			CString::equal_i(name, Config::default_normal_name))
			return;


		uint32 id = INVALID_ID;
		if (!remove_texture_reference(name, &id))
		{
			SHMERRORV("release - failed to release texture : '%s'.", name);
			return;
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

	static void texture_load_on_success(void* params)
	{
		TextureLoadParams* texture_params = (TextureLoadParams*)params;

		Texture old;
		Memory::copy_memory(texture_params->out_texture, &old, sizeof(old));
		
		ImageConfig* resource_data = (ImageConfig*)texture_params->image_resource.data;
		Renderer::texture_create(resource_data->pixels, &texture_params->temp_texture);

		Memory::copy_memory(&texture_params->temp_texture, texture_params->out_texture, sizeof(*texture_params->out_texture));

		Renderer::texture_destroy(&old);

		if (texture_params->current_generation == INVALID_ID)
			texture_params->out_texture->generation = 0;
		else
			texture_params->out_texture->generation = texture_params->current_generation + 1;

		SHMTRACEV("Successfully loaded texture '%s'.", texture_params->resource_name);

		ResourceSystem::unload(&texture_params->image_resource);
		Memory::free_memory(texture_params->resource_name);
	}

	static void texture_load_on_failure(void* params)
	{
		TextureLoadParams* texture_params = (TextureLoadParams*)params;

		SHMTRACEV("Failed to load texture '%s'.", texture_params->resource_name);

		ResourceSystem::unload(&texture_params->image_resource);
		if (texture_params->resource_name)
			Memory::free_memory(texture_params->resource_name);
	}

	static bool32 texture_load_job_start(void* params, void* results)
	{

		TextureLoadParams* load_params = (TextureLoadParams*)params;

		ImageResourceParams resource_params;
		resource_params.flip_y = true;

		if (!ResourceSystem::load(load_params->resource_name, ResourceType::IMAGE, &params, &load_params->image_resource))
		{
			SHMERRORV("load_texture - Failed to load image resources for texture '%s'", load_params->resource_name);
			return false;
		}

		ImageConfig* img_resource_data = (ImageConfig*)load_params->image_resource.data;
		load_params->temp_texture.channel_count = img_resource_data->channel_count;
		load_params->temp_texture.width = img_resource_data->width;
		load_params->temp_texture.height = img_resource_data->height;

		load_params->current_generation = load_params->out_texture->generation;
		load_params->out_texture->generation = INVALID_ID;

		uint8* pixels = img_resource_data->pixels;
		uint64 size = load_params->temp_texture.width * load_params->temp_texture.height * load_params->temp_texture.channel_count;
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

		CString::copy(Texture::max_name_length, load_params->temp_texture.name, load_params->resource_name);
		load_params->temp_texture.generation = INVALID_ID;
		load_params->temp_texture.flags = 0;
		load_params->temp_texture.flags |= has_transparency ? TextureFlags::HAS_TRANSPARENCY : 0;

		Memory::copy_memory(load_params, results, sizeof(TextureLoadParams));

		return true;

	}

	static bool32 load_texture(const char* texture_name, Texture* t)
	{
		TextureLoadParams params;
		uint32 name_length = CString::length(texture_name);
		params.resource_name = (char*)Memory::allocate(name_length + 1, AllocationTag::STRING);
		CString::copy(name_length + 1, params.resource_name, texture_name);
		params.out_texture = t;
		params.image_resource = {};
		params.current_generation = t->generation;
		
		JobSystem::JobInfo job = JobSystem::job_create(texture_load_job_start, texture_load_on_success, texture_load_on_failure, &params, sizeof(TextureLoadParams), sizeof(TextureLoadParams));
		JobSystem::submit(job);
		return true;
	}

	static bool32 load_cube_textures(const char* name, const char texture_names[6][Texture::max_name_length], Texture* t)
	{

		Buffer pixels = {};
		uint64 image_size = 0;

		for (uint32 i = 0; i < 6; i++)
		{

			ImageResourceParams params;
			params.flip_y = false;

			Resource img_resource;
			if (!ResourceSystem::load(texture_names[i], ResourceType::IMAGE, &params, &img_resource))
			{
				SHMERRORV("load_texture - Failed to load image resources for texture '%s'", texture_names[i]);
				return false;
			}			

			ImageConfig* img_resource_data = (ImageConfig*)img_resource.data;
			if (!pixels.data)
			{
				t->channel_count = img_resource_data->channel_count;
				t->width = img_resource_data->width;
				t->height = img_resource_data->height;

				t->generation = INVALID_ID;
				t->flags = 0;
				CString::copy(Texture::max_name_length, t->name, texture_names[i]);

				image_size = t->width * t->height * t->channel_count;
				pixels.init(image_size * 6, 0, AllocationTag::TEXTURE);			
			}
			else
			{
				if (t->width != img_resource_data->width || t->height != img_resource_data->height || t->channel_count != img_resource_data->channel_count)
				{
					SHMERROR("load_cube_textures - Cannot load cube textures since dimensions vary between textures!");
					pixels.free_data();
					return false;
				}
			}

			pixels.copy_memory(img_resource_data->pixels, image_size, image_size * i);

			ResourceSystem::unload(&img_resource);

		}

		Renderer::texture_create(pixels.data, t);
		pixels.free_data();
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
		system_state->default_texture.type = TextureType::TYPE_2D;
		system_state->default_texture.flags = 0;

		Renderer::texture_create(pixels, &system_state->default_texture);
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
		system_state->default_diffuse.type = TextureType::TYPE_2D;
		system_state->default_diffuse.flags = 0;
		Renderer::texture_create(diff_pixels, &system_state->default_diffuse);
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
		system_state->default_specular.type = TextureType::TYPE_2D;
		system_state->default_specular.flags = 0;
		Renderer::texture_create(spec_pixels, &system_state->default_specular);
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
		system_state->default_normal.type = TextureType::TYPE_2D;
		system_state->default_normal.flags = 0;
		Renderer::texture_create(normal_pixels, &system_state->default_normal);
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
		Renderer::texture_destroy(t);

		Memory::zero_memory(t, sizeof(t));
		t->id = INVALID_ID;
		t->generation = INVALID_ID;
	}

	static bool32 add_texture_reference(const char* name, TextureType type, bool32 auto_release, bool32 skip_load, uint32* out_texture_id)
	{

		*out_texture_id = INVALID_ID;
		TextureReference& ref = system_state->registered_texture_table.get_ref(name);
		if (ref.reference_count == 0)
				ref.auto_release = auto_release;
		ref.reference_count++;

		if (ref.handle == INVALID_ID)
		{
			for (uint32 i = 0; i < system_state->config.max_texture_count; i++)
			{
				if (system_state->registered_textures[i].id == INVALID_ID)
				{
					ref.handle = i;
					*out_texture_id = ref.handle;
					break;
				}
			}

			if (ref.handle == INVALID_ID)
			{
				SHMFATAL("Could not acquire new texture to texture system since no free slots are left!");
				return false;
			}

			Texture* t = &system_state->registered_textures[ref.handle];
			t->type = type;

			if (!skip_load)
			{
				if (type == TextureType::TYPE_CUBE)
				{
					char texture_names[6][Texture::max_name_length];

					CString::safe_print_s<const char*>(texture_names[0], Texture::max_name_length, "%s_r", name);
					CString::safe_print_s<const char*>(texture_names[1], Texture::max_name_length, "%s_l", name);
					CString::safe_print_s<const char*>(texture_names[2], Texture::max_name_length, "%s_u", name);
					CString::safe_print_s<const char*>(texture_names[3], Texture::max_name_length, "%s_d", name);
					CString::safe_print_s<const char*>(texture_names[4], Texture::max_name_length, "%s_f", name);
					CString::safe_print_s<const char*>(texture_names[5], Texture::max_name_length, "%s_b", name);

					if (!load_cube_textures(name, texture_names, t))
					{
						*out_texture_id = INVALID_ID;
						SHMERRORV("Failed to load texture: '%s'", name);
						return 0;
					}
				}
				else
				{
					if (!load_texture(name, t))
					{
						*out_texture_id = INVALID_ID;
						SHMERRORV("Failed to load texture: '%s'", name);
						return 0;
					}		
				}	

				t->id = ref.handle;
			}		

			t->id = ref.handle;
			SHMTRACEV("Texture '%s' did not exist yet. Created and ref count is now %i.", name, ref.reference_count);
		}
		else
		{
			*out_texture_id = ref.handle;
			SHMTRACEV("Texture '%s' already existed. ref count is now %i.", name, ref.reference_count);
		}

		return true;

	}

	static bool32 remove_texture_reference(const char* name, uint32* out_texture_id)
	{

		*out_texture_id = INVALID_ID;
		TextureReference& ref = system_state->registered_texture_table.get_ref(name);
		if (ref.reference_count == 0)
		{
			if (ref.auto_release)
			{
				SHMWARNV("Tried to release non-existent texture: '%s'", name);
				return false;
			}
			else
			{
				SHMWARN("Tried to release a texture where autorelease=false, but references was already 0.");
				return true;
			}		
		}

		ref.reference_count--;

		if (ref.reference_count == 0 && ref.auto_release)
		{
			Texture* t = &system_state->registered_textures[ref.handle];

			destroy_texture(t);

			ref.handle = INVALID_ID;
			ref.auto_release = false;

			SHMTRACEV("Released Texture '%s'. Texture unloaded because reference count == 0 and auto_release enabled.", name, ref.reference_count);
		}
		else
		{
			SHMTRACEV("Released Texture '%s'. ref count is now %i.", name, ref.reference_count);
		}

		return true;

	}
}