#include "ImageLoader.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb/stb_image.h"

namespace ResourceSystem
{

	static bool32 image_loader_load(ResourceLoader* loader, const char* name, Resource* out_resource)
	{

		const char* format = "%s%s%s%s";
		const int32 required_channel_count = 4;
		stbi_set_flip_vertically_on_load(true);
		char full_filepath[MAX_FILEPATH_LENGTH];

		String::safe_print_s<const char*, const char*, const char*, const char*>
			(full_filepath, MAX_FILEPATH_LENGTH, format, get_base_path(), loader->type_path, name, ".png");

		int32 width;
		int32 height;
		int32 channel_count;

		uint8* data = stbi_load(full_filepath, &width, &height, &channel_count, required_channel_count);

		const char* err_msg = stbi_failure_reason();
		if (err_msg)
		{
			SHMERRORV("image_loader_load - Failed to load file '%s' : %s", full_filepath, err_msg);
			stbi__err(0, 0);
			if (data)
				stbi_image_free(data);
			return false;
		}

		if (!data)
		{
			SHMERRORV("image_loader_load - Failed to load file '%s' - no stbi error thrown!", full_filepath);
			return false;
		}

		uint32 full_path_size = String::length(full_filepath) + 1;
		out_resource->full_path = (char*)Memory::allocate(full_path_size, true, AllocationTag::MAIN);
		String::copy(full_path_size, out_resource->full_path, full_filepath);

		ResourceDataImage* resource_data = (ResourceDataImage*)Memory::allocate(sizeof(ResourceDataImage), true, AllocationTag::MAIN);
		resource_data->pixels = data;
		resource_data->width = width;
		resource_data->height = height;
		resource_data->channel_count = required_channel_count;

		out_resource->data = resource_data;
		out_resource->data_size = sizeof(ResourceDataImage);
		out_resource->name = name;

		return true;

	}

	static void image_loader_unload(ResourceLoader* loader, Resource* resource)
	{
		if (resource->data)
			Memory::free_memory(resource->data, true, AllocationTag::MAIN);
		if (resource->full_path)
			Memory::free_memory(resource->full_path, true, AllocationTag::MAIN);

		resource->data = 0;
		resource->data_size = 0;
		resource->full_path = 0;
		resource->loader_id = INVALID_OBJECT_ID;
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