#include "ImageLoader.hpp"

#include "LoaderUtils.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "platform/FileSystem.hpp"
#include "utility/CString.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "vendor/stb/stb_image.h"

namespace ResourceSystem
{

	static bool32 image_loader_load(ResourceLoader* loader, const char* name, void* params, Resource* out_resource)
	{

		ImageResourceParams* load_params = (ImageResourceParams*)params;

		const char* format = "%s%s%s";
		const int32 required_channel_count = 4;
		stbi_set_flip_vertically_on_load_thread(load_params->flip_y);
		char full_filepath[MAX_FILEPATH_LENGTH];

		CString::safe_print_s<const char*, const char*, const char*>
			(full_filepath, MAX_FILEPATH_LENGTH, format, get_base_path(), loader->type_path, name);

		const uint32 valid_extension_count = 4;
		bool32 found = false;
		const char* extensions[valid_extension_count] = { ".tga", ".png", ".jpg", ".bmp" };
		char full_filepath_tmp[MAX_FILEPATH_LENGTH] = {};
		for (uint32 i = 0; i < valid_extension_count; i++)
		{			
			CString::copy(MAX_FILEPATH_LENGTH, full_filepath_tmp, full_filepath);
			CString::append(MAX_FILEPATH_LENGTH, full_filepath_tmp, extensions[i]);
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

		CString::copy(MAX_FILEPATH_LENGTH, full_filepath, full_filepath_tmp);

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

		Buffer raw_data(file_size, 0, AllocationTag::RESOURCE);
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

		out_resource->full_path = full_filepath;

		ImageConfig* resource_data = (ImageConfig*)Memory::allocate(sizeof(ImageConfig), AllocationTag::RESOURCE);
		resource_data->pixels = data;
		resource_data->width = width;
		resource_data->height = height;
		resource_data->channel_count = required_channel_count;

		out_resource->data = resource_data;
		out_resource->data_size = sizeof(ImageConfig);
		out_resource->name = name;

		return true;

	}

	static void image_loader_unload(ResourceLoader* loader, Resource* resource)
	{
	
		if (resource->data)
		{
			uint8* pixels = ((ImageConfig*)resource->data)->pixels;
			if (pixels)
				stbi_image_free(pixels);

			ImageConfig* data = (ImageConfig*)resource->data;
			(*data).~ImageConfig();
		}	

		resource_unload(loader, resource);
	}

	ResourceLoader image_resource_loader_create()
	{
		ResourceLoader loader;
		loader.type = ResourceType::IMAGE;
		loader.custom_type = 0;
		loader.load = image_loader_load;
		loader.unload = image_loader_unload;
		loader.type_path = "textures/";

		return loader;
	}

}
