#include "ImageLoader.hpp"

#include "LoaderUtils.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/CString.hpp"

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

		CString::safe_print_s<const char*, const char*, const char*, const char*>
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

		out_resource->full_path = full_filepath;

		ImageConfig* resource_data = (ImageConfig*)Memory::allocate(sizeof(ImageConfig), true, AllocationTag::MAIN);
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
			ImageConfig* data = (ImageConfig*)resource->data;
			(*data).~ImageConfig();
		}	

		resource_unload(loader, resource, AllocationTag::MAIN);
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
