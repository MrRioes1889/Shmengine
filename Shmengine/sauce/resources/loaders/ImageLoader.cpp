#include "ImageLoader.hpp"

#include "core/Engine.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "containers/Buffer.hpp"
#include "platform/FileSystem.hpp"
#include "utility/CString.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "vendor/stb/stb_image.h"

namespace ResourceSystem
{

	static const char* loader_type_path = "textures/";

	bool8 image_loader_load(const char* name, bool8 flip_y, ImageConfig* out_config)
	{

		const char* format = "%s%s%s";
		const int32 required_channel_count = 4;
		stbi_set_flip_vertically_on_load_thread(flip_y);
		char full_filepath[Constants::max_filepath_length];

		CString::safe_print_s<const char*, const char*, const char*>
			(full_filepath, Constants::max_filepath_length, format, Engine::get_assets_base_path(), loader_type_path, name);

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

		if (!found) {
			SHMERRORV("Image resource loader failed to find file '%s' with any valid extensions.", full_filepath_tmp);
			return false;
		}

		CString::copy(full_filepath_tmp,full_filepath, Constants::max_filepath_length);

		int32 width;
		int32 height;
		int32 channel_count;

		FileSystem::FileHandle f;
		if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f)) {
			SHMERRORV("Unable to read file: %s.", full_filepath);
			FileSystem::file_close(&f);
			return false;
		}

		uint32 file_size = FileSystem::get_file_size32(&f);
		if (!FileSystem::get_file_size32(&f)) {
			SHMERRORV("Unable to get size of file: %s.", full_filepath);
			FileSystem::file_close(&f);
			return false;
		}

		Buffer raw_data(file_size, 0, AllocationTag::Resource);
		uint32 bytes_read = 0;
		if (!FileSystem::read_all_bytes(&f, raw_data.data, (uint32)raw_data.size, &bytes_read))
		{
			SHMERRORV("Unable to read file: %s.", full_filepath);
			FileSystem::file_close(&f);
			raw_data.free_data();
			return false;
		}

		FileSystem::file_close(&f);
		uint8* data = stbi_load_from_memory((stbi_uc*)raw_data.data, (int)raw_data.size, &width, &height, &channel_count, required_channel_count);
		raw_data.free_data();
		if (!data)
		{
			SHMERRORV("Image resource loader failed to load file '%s'.", full_filepath);
			return false;
		}

		out_config->pixels = data;
		out_config->width = width;
		out_config->height = height;
		out_config->channel_count = required_channel_count;

		return true;

	}

	void image_loader_unload(ImageConfig* config)
	{
	
		if (config->pixels)
			stbi_image_free(config->pixels);
		config->pixels = 0;

	}

}
