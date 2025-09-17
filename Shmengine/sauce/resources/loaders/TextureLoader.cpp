#include "TextureLoader.hpp"

#include "core/Engine.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "containers/Buffer.hpp"
#include "platform/FileSystem.hpp"
#include "utility/CString.hpp"
#include "systems/TextureSystem.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_MALLOC(sz)           Memory::allocate(sz, AllocationTag::Resource)
#define STBI_REALLOC(p,newsz)     (p ? Memory::reallocate(newsz, p) : Memory::allocate(newsz, AllocationTag::Resource))
#define STBI_FREE(p)              if(p) Memory::free_memory(p)
#include "vendor/stb/stb_image.h"

namespace ResourceSystem
{
	static const char* loader_type_path = "textures/";

	bool8 texture_loader_load(const char* resource_name, bool8 flip_y, TextureResourceData* out_resource)
	{
		Buffer file_data = {};
		FileSystem::FileHandle f;
		uint32 file_size = 0;
		uint32 bytes_read = 0;
		uint32 pixel_buffer_size = 0;
		uint8* pixels_data = 0;
		int width, height, channel_count;

		const char* format = "%s%s%s";
		const int32 required_channel_count = 4;
		stbi_set_flip_vertically_on_load_thread(flip_y);
		char full_filepath[Constants::max_filepath_length];

		CString::safe_print_s<const char*, const char*, const char*>
			(full_filepath, Constants::max_filepath_length, format, Engine::get_assets_base_path(), loader_type_path, resource_name);

		const uint32 valid_extension_count = 4;
		bool8 found = false;
		const char* extensions[valid_extension_count] = { ".tga", ".png", ".jpg", ".bmp" };
		char full_filepath_tmp[Constants::max_filepath_length] = {};
		for (uint32 i = 0; i < valid_extension_count; i++)
		{			
			CString::copy(full_filepath, full_filepath_tmp, Constants::max_filepath_length);
			CString::append(full_filepath_tmp, Constants::max_filepath_length, extensions[i]);
			if (FileSystem::file_exists(full_filepath_tmp))
			{
				found = true;
				break;
			}
		}

		goto_if_log(!found, fail, "Image resource loader failed to find file '%s' with any valid extensions.", full_filepath_tmp);
		CString::copy(full_filepath_tmp,full_filepath, Constants::max_filepath_length);

		goto_if_log(!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f), fail, "Unable to read file: %s.", full_filepath);

		file_size = FileSystem::get_file_size32(&f);
		goto_if_log(!FileSystem::get_file_size32(&f), fail, "Unable to get size of file: %s.", full_filepath);

		file_data.init(file_size, 0, AllocationTag::Resource);
		bytes_read = 0;
		goto_if_log(!FileSystem::read_all_bytes(&f, file_data.data, (uint32)file_data.size, &bytes_read), fail, "Unable to read file: %s.", full_filepath);

		FileSystem::file_close(&f);
		pixels_data = stbi_load_from_memory((stbi_uc*)file_data.data, (int)file_data.size, &width, &height, &channel_count, required_channel_count);
		goto_if_log(!pixels_data, fail, "Image resource loader failed to load file '%s'.", full_filepath);

		out_resource->width = width;
		out_resource->height = height;
		out_resource->channel_count = required_channel_count;
		out_resource->flipped_y = flip_y;
		pixel_buffer_size = out_resource->width * out_resource->height * out_resource->channel_count;
		out_resource->pixels.init(pixel_buffer_size, 0, AllocationTag::Resource, pixels_data);

		file_data.free_data();
		return true;

	fail:
		FileSystem::file_close(&f);
		file_data.free_data();
		texture_loader_unload(out_resource);
		return false;
	}

	void texture_loader_unload(TextureResourceData* resource)
	{
		if (resource->pixels.data)
			stbi_image_free(resource->pixels.data);
		resource->pixels.free_data();
	}

	TextureConfig texture_loader_get_config_from_resource(TextureResourceData* resource)
	{
		TextureConfig config =
		{
			.channel_count = resource->channel_count,
			.width = resource->width,
			.height = resource->height,
			.pixels = (uint8*)resource->pixels.data
		};
		return config;
	}

}
