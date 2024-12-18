#include "MaterialLoader.hpp"

#include "systems/MaterialSystem.hpp"
#include "systems/ResourceSystem.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/CString.hpp"
#include "platform/FileSystem.hpp"
#include "containers/Sarray.hpp"

namespace ResourceSystem
{

    static const char* loader_type_path = "materials/";

	bool32 material_loader_load(const char* name, void* params, MaterialConfig* out_config)
	{

		const char* format = "%s%s%s%s";
		char full_filepath[MAX_FILEPATH_LENGTH];

		CString::safe_print_s<const char*, const char*, const char*, const char*>
			(full_filepath, MAX_FILEPATH_LENGTH, format, get_base_path(), loader_type_path, name, ".shmt");

		FileSystem::FileHandle f;
		if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
		{
			SHMERRORV("material_loader_load - Failed to open file for loading material '%s'", full_filepath);
			return false;
		}

		out_config->auto_release = true;
        out_config->shader_name = "Builtin.Material";
		out_config->diffuse_color = VEC4F_ONE;
		out_config->shininess = 32.0f;
		CString::empty(out_config->diffuse_map_name);
		CString::empty(out_config->specular_map_name);
		CString::empty(out_config->normal_map_name);
		CString::copy(max_material_name_length, out_config->name, name);

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

        String var_name;
        String value;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content.c_str(), line, &continue_ptr))
        {
            // Trim the string.

            line.trim();

            // Get the trimmed length.
            line_length = line.len();

            // Skip blank lines and comments.
            if (line_length < 1 || line[0] == '#') 
            {
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

            mid(var_name, line.c_str(), 0, equal_index);
            var_name.trim();

            mid(value, line.c_str(), equal_index + 1);
            value.trim();

            // Process the variable.
            if (var_name.equal_i("version")) 
            {
                // TODO: version
            }
            else if (var_name.equal_i("name")) 
            {
                CString::copy(max_material_name_length, out_config->name, value.c_str());
            }
            else if (var_name.equal_i("shader")) 
            {
                out_config->shader_name = value;             
            }
            else if (var_name.equal_i("diffuse_map_name")) 
            {
                CString::copy(max_texture_name_length, out_config->diffuse_map_name, value.c_str());
            }
            else if (var_name.equal_i("specular_map_name"))
            {
                CString::copy(max_texture_name_length, out_config->specular_map_name, value.c_str());
            }
            else if (var_name.equal_i("normal_map_name"))
            {
                CString::copy(max_texture_name_length, out_config->normal_map_name, value.c_str());
            }
            else if (var_name.equal_i("diffuse_color"))
            {
                // Parse the colour
                if (!CString::parse(value.c_str(), out_config->diffuse_color))
                {
                    SHMWARNV("Error parsing diffuse_colour in file '%s'. Using default of white instead.", full_filepath);
                }
            }
            else if (var_name.equal_i("shininess")) 
            {
                // Parse the colour
                if (!CString::parse_f32(value.c_str(), out_config->shininess)) 
                {
                    SHMWARNV("Error parsing shininess in file '%s'. Using default of 32.0 instead.", full_filepath);
                }
            }

            line_number++;
        }

        FileSystem::file_close(&f);

		return true;

	}

	void material_loader_unload(MaterialConfig* config)
	{
        config->shader_name.free_data();
	}

}