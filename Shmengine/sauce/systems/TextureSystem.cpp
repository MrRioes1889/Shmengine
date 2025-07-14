#include "TextureSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "core/Memory.hpp"
#include "containers/Hashtable.hpp"
#include "containers/Sarray.hpp"
#include "platform/Platform.hpp"

#include "renderer/RendererFrontend.hpp"

#include "resources/loaders/ImageLoader.hpp"
#include "systems/JobSystem.hpp"


namespace TextureSystem
{

	struct TextureLoadParams
	{
		char* resource_name;
		Texture* out_texture;
		Texture temp_texture;
		ImageConfig config;
	};

	struct TextureReference
	{
		TextureId id;
		uint16 reference_count;
		bool8 auto_release;
	};

	struct SystemState
	{
		Texture default_texture;
		Texture default_diffuse;
		Texture default_specular;
		Texture default_normal;

		Sarray<Texture> textures;
		HashtableRH<TextureReference> lookup_table;

		uint32 textures_loading_count;
	};	

	static SystemState* system_state = 0;
	
	static bool32 load_texture(const char* texture_name, Texture* t);
	static bool32 load_cube_textures(const char* name, const char texture_names[6][Constants::max_texture_name_length], Texture* t);
	static void destroy_texture(Texture* t);

	static void create_default_textures();
	static void destroy_default_textures();

	static bool8 add_reference(const char* name, bool8 auto_release, TextureId* out_texture_id, bool8* out_load);
	static TextureId remove_reference(const char* name);

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->textures_loading_count = 0;

		uint64 texture_array_size = system_state->textures.get_external_size_requirement(sys_config->max_texture_count);
		void* texture_array_data = allocator_callback(allocator, texture_array_size);
		system_state->textures.init(sys_config->max_texture_count, 0, AllocationTag::ARRAY, texture_array_data);

		uint64 hashtable_data_size = system_state->lookup_table.get_external_size_requirement(sys_config->max_texture_count);
		void* hashtable_data = allocator_callback(allocator, hashtable_data_size);
		system_state->lookup_table.init(sys_config->max_texture_count, HashtableOAFlag::ExternalMemory, AllocationTag::DICT, hashtable_data);

		for (uint32 i = 0; i < sys_config->max_texture_count; i++)
			system_state->textures[i].id.invalidate();

		create_default_textures();

		return true;
	}

	void system_shutdown(void* state)
	{
		if (system_state)
		{
			for (uint32 i = 0; i < system_state->textures.capacity; i++)
			{
				if (system_state->textures[i].id.is_valid())
					destroy_texture(&system_state->textures[i]);
			}

			destroy_default_textures();
			system_state->textures.free_data();
			system_state->lookup_table.free_data();
			system_state = 0;
		}
	}

	Texture* acquire(const char* name, bool8 auto_release)
	{
		
		if (CString::equal_i(name, SystemConfig::default_name))
			return &system_state->default_texture;

		if (CString::equal_i(name, SystemConfig::default_diffuse_name))
			return &system_state->default_diffuse;

		if (CString::equal_i(name, SystemConfig::default_specular_name))
			return &system_state->default_specular;

		if (CString::equal_i(name, SystemConfig::default_normal_name))
			return &system_state->default_normal;

		TextureId id = TextureId::invalid_value;
		bool8 load = false;
		if (!add_reference(name, auto_release, &id, &load))
		{
			SHMERRORV("acquire - failed to obtain new id for texture: '%s'.", name);
			return 0;
		}
		Texture* t = &system_state->textures[id];

		if (load && !load_texture(name, t))
		{
			destroy_texture(t);
			SHMERRORV("Failed to load texture: '%s'", name);
			return 0;
		}		

		return t;
	}

	Texture* acquire_cube(const char* name, bool8 auto_release)
	{
		TextureId id = TextureId::invalid_value;
		bool8 load = false;
		if (!add_reference(name, auto_release, &id, &load))
		{
			SHMERRORV("acquire - failed to obtain new id for texture: '%s'.", name);
			return 0;
		}
		Texture* t = &system_state->textures[id];

		char texture_names[6][Constants::max_texture_name_length];
		CString::safe_print_s<const char*>(texture_names[0], Constants::max_texture_name_length, "%s_r", name);
		CString::safe_print_s<const char*>(texture_names[1], Constants::max_texture_name_length, "%s_l", name);
		CString::safe_print_s<const char*>(texture_names[2], Constants::max_texture_name_length, "%s_u", name);
		CString::safe_print_s<const char*>(texture_names[3], Constants::max_texture_name_length, "%s_d", name);
		CString::safe_print_s<const char*>(texture_names[4], Constants::max_texture_name_length, "%s_f", name);
		CString::safe_print_s<const char*>(texture_names[5], Constants::max_texture_name_length, "%s_b", name);

		if (!load_cube_textures(name, texture_names, t))
		{
			destroy_texture(t);
			SHMERRORV("Failed to load texture: '%s'", name);
			return 0;
		}

		return t;
	}

	Texture* acquire_writable(const char* name, uint32 width, uint32 height, uint8 channel_count, bool8 has_transparency)
	{
		TextureId id = TextureId::invalid_value;
		bool8 load = false;
		if (!add_reference(name, false, &id, &load))
		{
			SHMERRORV("acquire_writable - failed to obtain new id for texture: '%s'.", name);
			return 0;
		}

		Texture* t = &system_state->textures[id];
		t->id = id;
		CString::copy(name, t->name, Constants::max_texture_name_length);
		t->width = width;
		t->height = height;
		t->channel_count = channel_count;
		t->type = TextureType::Plane;
		t->flags = 0;
		t->flags |= has_transparency ? TextureFlags::HasTransparency : 0;
		t->flags |= TextureFlags::IsWritable;
		Renderer::texture_create_writable(t);

		return t;

	}

	bool8 wrap_internal(const char* name, uint32 width, uint32 height, uint8 channel_count, bool8 has_transparency, bool8 is_writable, bool8 register_texture, void* internal_data, uint64 internal_data_size, Texture* out_texture)
	{

		TextureId id = TextureId::invalid_value;
		bool8 load = false;
		Texture* t = 0;

		if (register_texture)
		{

			if (!add_reference(name, false, &id, &load))
			{
				SHMERRORV("wrap_internal - failed to obtain new id for texture: '%s'.", name);
				return false;
			}

			t = &system_state->textures[id];
		}
		else
		{
			// TODO: Think about whether this is necessary
			if (out_texture)
				t = out_texture;
			else
				t = (Texture*)Memory::allocate(sizeof(Texture), AllocationTag::TEXTURE);

			SHMTRACEV("wrap_internal created texture '%s', but not registering, resulting in an allocation. It is up to the caller to free this memory.", name);
		}

		t->id = id;
		CString::copy(name, t->name, Constants::max_texture_name_length);
		t->width = width;
		t->height = height;
		t->channel_count = channel_count;
		t->type = TextureType::Plane;
		t->flags = 0;
		t->flags |= has_transparency ? TextureFlags::HasTransparency : 0;
		t->flags |= TextureFlags::IsWritable;
		t->flags |= TextureFlags::IsWrapped;
		t->internal_data.init(internal_data_size, 0, AllocationTag::TEXTURE, internal_data);

		return true;

	}

	bool8 set_internal(Texture* t, void* internal_data, uint64 internal_data_size)
	{	
		t->internal_data.init(internal_data_size, 0, AllocationTag::TEXTURE, internal_data);
		return true;
	}

	bool8 resize(Texture* t, uint32 width, uint32 height, bool8 regenerate_internal_data)
	{

		if (!(t->flags & TextureFlags::IsWritable))
		{
			SHMWARNV("resize - Non-writable texture '%s' cannot be resized!", t->name);
			return false;
		}

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

		if (CString::equal_i(name, SystemConfig::default_name) ||
			CString::equal_i(name, SystemConfig::default_diffuse_name) || 
			CString::equal_i(name, SystemConfig::default_specular_name) || 
			CString::equal_i(name, SystemConfig::default_normal_name))
			return;

		TextureId destroy_id = remove_reference(name);
		if (destroy_id.is_valid())
		{
			Texture* t = &system_state->textures[destroy_id];
			destroy_texture(t);
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
		TextureLoadParams* texture_params = (TextureLoadParams*)params;

		Texture old;
		Memory::copy_memory(texture_params->out_texture, &old, sizeof(old));
		
		ImageConfig* config = (ImageConfig*)&texture_params->config;
		Renderer::texture_create(config->pixels, &texture_params->temp_texture);

		Memory::copy_memory(&texture_params->temp_texture, texture_params->out_texture, sizeof(*texture_params->out_texture));
		Renderer::texture_destroy(&old);
		texture_params->out_texture->flags |= TextureFlags::IsLoaded;

		SHMTRACEV("Successfully loaded texture '%s'.", texture_params->resource_name);

		ResourceSystem::image_loader_unload(config);
		Memory::free_memory(texture_params->resource_name);

		system_state->textures_loading_count--;
	}

	static void texture_load_on_failure(void* params)
	{
		TextureLoadParams* texture_params = (TextureLoadParams*)params;

		SHMTRACEV("Failed to load texture '%s'.", texture_params->resource_name);

		ResourceSystem::image_loader_unload(&texture_params->config);
		if (texture_params->resource_name)
			Memory::free_memory(texture_params->resource_name);

		system_state->textures_loading_count--;
	}

	static bool32 texture_load_job_start(void* params, void* results)
	{

		TextureLoadParams* load_params = (TextureLoadParams*)params;

		ImageResourceParams resource_params;
		resource_params.flip_y = true;

		ImageConfig* config = &load_params->config;
		if (!ResourceSystem::image_loader_load(load_params->resource_name, &resource_params, config))
		{
			SHMERRORV("load_texture - Failed to load image resources for texture '%s'", load_params->resource_name);
			return false;
		}

		load_params->temp_texture.channel_count = config->channel_count;
		load_params->temp_texture.width = config->width;
		load_params->temp_texture.height = config->height;
		load_params->temp_texture.type = TextureType::Plane;

		load_params->out_texture->flags &= ~TextureFlags::IsLoaded;

		uint8* pixels = config->pixels;
		uint64 size = load_params->temp_texture.width * load_params->temp_texture.height * load_params->temp_texture.channel_count;
		bool32 has_transparency = false;
		for (uint64 i = 0; i < size; i += config->channel_count)
		{
			uint8 a = pixels[i + 3];
			if (a < 255)
			{
				has_transparency = true;
				break;
			}
		}

		CString::copy(load_params->resource_name, load_params->temp_texture.name, Constants::max_texture_name_length);
		load_params->temp_texture.flags = 0;
		load_params->temp_texture.flags |= has_transparency ? TextureFlags::HasTransparency : 0;

		Memory::copy_memory(load_params, results, sizeof(TextureLoadParams));
		Memory::zero_memory(load_params, sizeof(TextureLoadParams));

		return true;

	}

	static bool32 load_texture(const char* texture_name, Texture* t)
	{	
		system_state->textures_loading_count++;
		JobSystem::JobInfo job = JobSystem::job_create(texture_load_job_start, texture_load_on_success, texture_load_on_failure, sizeof(TextureLoadParams), sizeof(TextureLoadParams));
		TextureLoadParams* params = (TextureLoadParams*)job.params;
		uint32 name_length = CString::length(texture_name);
		params->resource_name = (char*)Memory::allocate(name_length + 1, AllocationTag::STRING);
		CString::copy(texture_name, params->resource_name, name_length + 1);
		params->out_texture = t;
		params->config = {};
		JobSystem::submit(job);
		return true;
	}

	static bool32 load_cube_textures(const char* name, const char texture_names[6][Constants::max_texture_name_length], Texture* t)
	{

		Buffer pixels = {};
		uint64 image_size = 0;

		for (uint32 i = 0; i < 6; i++)
		{

			ImageResourceParams params;
			params.flip_y = false;

			ImageConfig img_resource_data;
			if (!ResourceSystem::image_loader_load(texture_names[i], &params, &img_resource_data))
			{
				SHMERRORV("load_texture - Failed to load image resources for texture '%s'", texture_names[i]);
				return false;
			}

			if (!pixels.data)
			{
				t->channel_count = img_resource_data.channel_count;
				t->width = img_resource_data.width;
				t->height = img_resource_data.height;
				t->type = TextureType::Cube;

				t->flags = 0;
				CString::copy(texture_names[i], t->name, Constants::max_texture_name_length);

				image_size = t->width * t->height * t->channel_count;
				pixels.init(image_size * 6, 0, AllocationTag::TEXTURE);			
			}
			else
			{
				if (t->width != img_resource_data.width || t->height != img_resource_data.height || t->channel_count != img_resource_data.channel_count)
				{
					SHMERROR("load_cube_textures - Cannot load cube textures since dimensions vary between textures!");
					pixels.free_data();
					return false;
				}
			}

			pixels.copy_memory(img_resource_data.pixels, image_size, image_size * i);

			ResourceSystem::image_loader_unload(&img_resource_data);

		}

		Renderer::texture_create(pixels.data, t);
		t->flags |= TextureFlags::IsLoaded;
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

		CString::copy(SystemConfig::default_name, system_state->default_texture.name, Constants::max_texture_name_length);
		system_state->default_texture.width = tex_dim;
		system_state->default_texture.height = tex_dim;
		system_state->default_texture.channel_count = 4;
		system_state->default_texture.id.invalidate();
		system_state->default_texture.type = TextureType::Plane;
		system_state->default_texture.flags = 0;

		Renderer::texture_create(pixels, &system_state->default_texture);

		// Diffuse texture.
		SHMTRACE("Creating default diffuse texture...");
		uint8 diff_pixels[16 * 16 * 4];
		Memory::set_memory(diff_pixels, 0xFF, sizeof(diff_pixels));
		CString::copy(SystemConfig::default_diffuse_name, system_state->default_diffuse.name, Constants::max_texture_name_length);
		system_state->default_diffuse.id.invalidate();
		system_state->default_diffuse.width = 16;
		system_state->default_diffuse.height = 16;
		system_state->default_diffuse.channel_count = 4;
		system_state->default_diffuse.type = TextureType::Plane;
		system_state->default_diffuse.flags = 0;
		Renderer::texture_create(diff_pixels, &system_state->default_diffuse);

		// Specular texture.
		SHMTRACE("Creating default specular texture...");
		uint8 spec_pixels[16 * 16 * 4];
		// Default spec map is black (no specular)
		Memory::zero_memory(spec_pixels, sizeof(spec_pixels));
		CString::copy(SystemConfig::default_specular_name, system_state->default_specular.name, Constants::max_texture_name_length);
		system_state->default_specular.id.invalidate();
		system_state->default_specular.width = 16;
		system_state->default_specular.height = 16;
		system_state->default_specular.channel_count = 4;
		system_state->default_specular.type = TextureType::Plane;
		system_state->default_specular.flags = 0;
		Renderer::texture_create(spec_pixels, &system_state->default_specular);

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
		system_state->default_normal.id.invalidate();
		system_state->default_normal.width = 16;
		system_state->default_normal.height = 16;
		system_state->default_normal.channel_count = 4;
		system_state->default_normal.type = TextureType::Plane;
		system_state->default_normal.flags = 0;
		Renderer::texture_create(normal_pixels, &system_state->default_normal);

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
		system_state->lookup_table.remove_entry(t->name);
		Renderer::texture_destroy(t);

		t->id.invalidate();
		t->flags = 0;
		t->name[0] = 0;
	}

	static bool8 add_reference(const char* name, bool8 auto_release, TextureId* out_texture_id, bool8* out_load)
	{
		*out_texture_id = TextureId::invalid_value;
		*out_load = false;
		TextureReference* ref = system_state->lookup_table.get(name);
		if (ref)
		{
			ref->reference_count++;
			*out_texture_id = ref->id;
			return true;
		}

		TextureId new_id = TextureId::invalid_value;
		for (uint16 i = 0; i < system_state->textures.capacity; i++)
		{
			if (!system_state->textures[i].id.is_valid())
			{
				new_id = i;
				break;
			}
		}

		if (!new_id.is_valid())
		{
			SHMFATAL("Could not acquire new texture to texture system since no free slots are left!");
			return false;
		}

		Texture* t = &system_state->textures[new_id];

		t->id = new_id;
		CString::copy(name, t->name, Constants::max_texture_name_length);
		TextureReference new_ref = { .id = new_id, .reference_count = 1, .auto_release = auto_release };
		ref = system_state->lookup_table.set_value(t->name, new_ref);
		*out_texture_id = ref->id;
		*out_load = true;

		return true;
	}

	static TextureId remove_reference(const char* name)
	{
		TextureReference* ref = system_state->lookup_table.get(name);
		if (!ref)
		{
			SHMWARNV("Tried to release non-existent texture: '%s'", name);
			return TextureId::invalid_value;
		}
		else if (!ref->reference_count)
		{
			SHMWARN("Tried to release a texture where autorelease=false, but references was already 0.");
			return TextureId::invalid_value;
		}

		ref->reference_count--;
		if (ref->reference_count == 0 && ref->auto_release)
		{
			return ref->id;
		}

		return TextureId::invalid_value;
	}
}