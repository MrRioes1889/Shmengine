#include "MaterialLoader.hpp"

#include "LoaderUtils.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/CString.hpp"
#include "platform/FileSystem.hpp"
#include "containers/Sarray.hpp"

namespace ResourceSystem
{

	static bool32 material_loader_load(ResourceLoader* loader, const char* name, Resource* out_resource)
	{

		const char* format = "%s%s%s%s";
		char full_filepath[MAX_FILEPATH_LENGTH];

		CString::safe_print_s<const char*, const char*, const char*, const char*>
			(full_filepath, MAX_FILEPATH_LENGTH, format, get_base_path(), loader->type_path, name, ".mt");

		FileSystem::FileHandle f;
		if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
		{
			SHMERRORV("material_loader_load - Failed to open file for loading material '%s'", full_filepath);
			return false;
		}

		MaterialConfig tmp_res_data;
		tmp_res_data.auto_release = true;
        tmp_res_data.shader_name = "Builtin.Material";
		tmp_res_data.diffuse_color = VEC4F_ONE;
		CString::empty(tmp_res_data.diffuse_map_name);
		CString::copy(Material::max_name_length, tmp_res_data.name, name);

        uint32 file_size = FileSystem::get_file_size32(&f);
        String file_content(file_size + 1);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(&f, file_content, &bytes_read))
        {
            SHMERRORV("material_loader_load - failed to read from file: '%s'.", full_filepath);
            return false;
        }

        // Read each line of the file.
        String line(512);
        uint64 line_length = 0;
        uint32 line_number = 1;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content, line, &continue_ptr)) {
            // Trim the string.

            line.trim();

            // Get the trimmed length.
            line_length = line.len();

            // Skip blank lines and comments.
            if (line_length < 1 || line[0] == '#') {
                line_number++;
                continue;
            }

            // Split into var/value
            int32 equal_index = line.index_of('=');
            if (equal_index == -1) {
                SHMWARNV("Potential formatting issue found in file '%s': '=' token not found. Skipping line %ui.", full_filepath, line_number);
                line_number++;
                continue;
            }

            String var_name = mid(line, 0, equal_index);
            var_name.trim();

            String value = mid(line, equal_index + 1);
            value.trim();

            // Process the variable.
            if (var_name.equal_i("version")) {
                // TODO: version
            }
            else if (var_name.equal_i("name")) {
                CString::copy(Material::max_name_length, tmp_res_data.name, value.c_str());
            }
            else if (var_name.equal_i("shader")) {
                tmp_res_data.shader_name = value;             
            }
            else if (var_name.equal_i("diffuse_map_name")) {
                CString::copy(Texture::max_name_length, tmp_res_data.diffuse_map_name, value.c_str());
            }
            else if (var_name.equal_i("diffuse_color")) {
                // Parse the colour
                if (!CString::parse(value.c_str(), tmp_res_data.diffuse_color)) {
                    SHMWARNV("Error parsing diffuse_colour in file '%s'. Using default of white instead.", full_filepath);
                }
            }

            // TODO: more fields.

            // Clear the line buffer.
            line_number++;
        }

        FileSystem::file_close(&f);

		out_resource->full_path = full_filepath;

		MaterialConfig* resource_data = (MaterialConfig*)Memory::allocate(sizeof(MaterialConfig), true, AllocationTag::MAIN);
        *resource_data = tmp_res_data;

		out_resource->data = resource_data;
		out_resource->data_size = sizeof(MaterialConfig);
		out_resource->name = name;

		return true;

	}

	static void material_loader_unload(ResourceLoader* loader, Resource* resource)
	{
        if (resource->data)
        {
            MaterialConfig* data = (MaterialConfig*)resource->data;
            (*data).~MaterialConfig();
        }

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