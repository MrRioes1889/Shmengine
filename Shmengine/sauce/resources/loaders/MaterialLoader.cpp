#include "MaterialLoader.hpp"

#include "LoaderUtils.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"
#include "containers/Sarray.hpp"

namespace ResourceSystem
{

	static bool32 material_loader_load(ResourceLoader* loader, const char* name, Resource* out_resource)
	{

		const char* format = "%s%s%s%s";
		char full_filepath[MAX_FILEPATH_LENGTH];

		String::safe_print_s<const char*, const char*, const char*, const char*>
			(full_filepath, MAX_FILEPATH_LENGTH, format, get_base_path(), loader->type_path, name, ".mt");

		FileSystem::FileHandle f;
		if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
		{
			SHMERRORV("material_loader_load - Failed to open file for loading material '%s'", full_filepath);
			return false;
		}

		ResourceDataMaterial tmp_res_data;
		tmp_res_data.auto_release = true;
		tmp_res_data.type = MaterialType::WORLD;
		tmp_res_data.diffuse_color = VEC4F_ONE;
		String::empty(tmp_res_data.diffuse_map_name);
		String::copy(Material::max_name_length, tmp_res_data.name, name);

        uint32 file_size = FileSystem::get_file_size32(&f);
        Sarray<char> file_content(file_size + 1, AllocationTag::TRANSIENT);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(&f, file_content.data, file_content.count, &bytes_read))
        {
            SHMERRORV("material_loader_load - failed to read from file: '%s'.", full_filepath);
            return false;
        }

        // Read each line of the file.
        const uint32 line_buf_length = 512;
        char line_buf[line_buf_length] = "";
        char* p = &line_buf[0];
        uint64 line_length = 0;
        uint32 line_number = 1;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content.data, line_buf, line_buf_length, &continue_ptr)) {
            // Trim the string.
            char* line = String::trim(line_buf);

            // Get the trimmed length.
            line_length = String::length(line);

            // Skip blank lines and comments.
            if (line_length < 1 || line[0] == '#') {
                line_number++;
                continue;
            }

            // Split into var/value
            int32 equal_index = String::index_of(line, '=');
            if (equal_index == -1) {
                SHMWARNV("Potential formatting issue found in file '%s': '=' token not found. Skipping line %ui.", full_filepath, line_number);
                line_number++;
                continue;
            }

            // Assume a max of 64 characters for the variable name.
            char raw_var_name[64] = {};
            String::mid(64, raw_var_name, line, 0, equal_index);
            char* trimmed_var_name = String::trim(raw_var_name);

            // Assume a max of 511-65 (446) for the max length of the value to account for the variable name and the '='.
            char raw_value[446];
            Memory::zero_memory(raw_value, sizeof(char) * 446);
            String::mid(446, raw_value, line, equal_index + 1, -1);  // Read the rest of the line
            char* trimmed_value = String::trim(raw_value);

            // Process the variable.
            if (String::equal_i(trimmed_var_name, "version")) {
                // TODO: version
            }
            else if (String::equal_i(trimmed_var_name, "name")) {
                String::copy(Material::max_name_length, tmp_res_data.name, trimmed_value);
            }
            else if (String::equal_i(trimmed_var_name, "type")) {
                if (String::equal_i(trimmed_value, "ui"))
                {
                    tmp_res_data.type = MaterialType::UI;
                }
                else if (String::equal_i(trimmed_value, "world"))
                {
                    tmp_res_data.type = MaterialType::WORLD;
                }
                else
                {
                    SHMWARNV("Error parsing material type in file '%s'. Using default of 'world' instead.", full_filepath);
                }
                
            }
            else if (String::equal_i(trimmed_var_name, "diffuse_map_name")) {
                String::copy(Texture::max_name_length, tmp_res_data.diffuse_map_name, trimmed_value);
            }
            else if (String::equal_i(trimmed_var_name, "diffuse_color")) {
                // Parse the colour
                if (!String::parse(trimmed_value, tmp_res_data.diffuse_color)) {
                    SHMWARNV("Error parsing diffuse_colour in file '%s'. Using default of white instead.", full_filepath);
                }
            }

            // TODO: more fields.

            // Clear the line buffer.
            Memory::zero_memory(line_buf, sizeof(char) * line_buf_length);
            line_number++;
        }

        FileSystem::file_close(&f);

		uint32 full_path_size = String::length(full_filepath) + 1;
		out_resource->full_path = (char*)Memory::allocate(full_path_size, true, AllocationTag::MAIN);
		String::copy(full_path_size, out_resource->full_path, full_filepath);

		ResourceDataMaterial* resource_data = (ResourceDataMaterial*)Memory::allocate(sizeof(ResourceDataMaterial), true, AllocationTag::MAIN);
        *resource_data = tmp_res_data;

		out_resource->data = resource_data;
		out_resource->data_size = sizeof(ResourceDataMaterial);
		out_resource->name = name;

		return true;

	}

	static void material_loader_unload(ResourceLoader* loader, Resource* resource)
	{
        resource_unload(loader, resource, AllocationTag::MAIN);
	}

	ResourceLoader material_resource_loader_create()
	{
		ResourceLoader loader;
		loader.type = ResourceType::MATERIAL;
		loader.custom_type = 0;
		loader.load = material_loader_load;
		loader.unload = material_loader_unload;
		loader.type_path = "materials/";

		return loader;
	}

}