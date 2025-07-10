#include "TerrainLoader.hpp"

#include "core/Engine.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "utility/math/Transform.hpp"
#include "platform/FileSystem.hpp"
#include "renderer/RendererTypes.hpp"
#include "renderer/RendererGeometry.hpp"
#include "systems/GeometrySystem.hpp"
#include "resources/Mesh.hpp"

namespace ResourceSystem
{

    static const char* loader_type_path = "terrains/";

#define PARSE_VALUE(s, v) if (!CString::parse(s, v)) \
    { \
        SHMERRORV("Failed parsing value for %s on line %u", s, line_number); \
        success = false; \
        continue; \
    }

    bool32 terrain_loader_load(const char* name, void* params, TerrainResourceData* out_resource)
    {

        const char* format = "%s%s%s%s";
        char full_filepath[Constants::max_filepath_length];

        CString::safe_print_s<const char*, const char*, const char*, const char*>
            (full_filepath, Constants::max_filepath_length, format, Engine::get_assets_base_path(), loader_type_path, name, ".shmter");

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("Failed to open file for loading terrain '%s'", full_filepath);
            return false;
        }

        uint32 file_size = FileSystem::get_file_size32(&f);
        String file_content(file_size + 1);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(&f, file_content, &bytes_read))
        {
            SHMERRORV("Failed to read from file: '%s'.", full_filepath);
            return false;
        }

        enum class ParserScope
        {
            TERRAIN
        };

        ParserScope scope = ParserScope::TERRAIN;
        // Read each line of the file.
        String line(512);
        uint64 line_length = 0;

        String var_name;
        String value;

        bool32 success = true;

        out_resource->name[0] = 0;
        out_resource->heightmap_name[0] = 0;

        out_resource->tile_count_x = 100;
        out_resource->tile_count_z = 100;
        out_resource->tile_scale_x = 1.0f;
        out_resource->tile_scale_z = 1.0f;
        out_resource->scale_y = 1.0f;

        out_resource->sub_materials_count = 0;

        uint32 line_number = 1;
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

            if (scope == ParserScope::TERRAIN)
            {
                // Process the variable.
                if (var_name.equal_i("version"))
                {
                }
                else if (var_name.equal_i("name"))
                {
                    CString::copy(value.c_str(), out_resource->name, Constants::max_terrain_name_length);
                }
                else if (var_name.equal_i("heightmap_resource_name"))
                {
                    CString::copy(value.c_str(), out_resource->heightmap_name, Constants::max_texture_name_length);
                }
                else if (var_name.equal_i("tile_count_x"))
                {
                    PARSE_VALUE(value.c_str(), &out_resource->tile_count_x);
                }
                else if (var_name.equal_i("tile_count_z"))
                {
                    PARSE_VALUE(value.c_str(), &out_resource->tile_count_z);
                }
                else if (var_name.equal_i("scale_x"))
                {
                    PARSE_VALUE(value.c_str(), &out_resource->tile_scale_x);
                }
                else if (var_name.equal_i("scale_z"))
                {
                    PARSE_VALUE(value.c_str(), &out_resource->tile_scale_z);
                }
                else if (var_name.equal_i("scale_y"))
                {
                    PARSE_VALUE(value.c_str(), &out_resource->scale_y);
                }
                else if (var_name.equal_i("material"))
                {
                    if (out_resource->sub_materials_count < Constants::max_terrain_materials_count)
                        CString::copy(value.c_str(), out_resource->sub_material_names[out_resource->sub_materials_count++].name, Constants::max_material_name_length);
                }
            }
            

            line_number++;
        }

        FileSystem::file_close(&f);

        if (!out_resource->name[0])
        {
            SHMERRORV("Unsufficient data describing terrain in file %s.", name);
            success = false;
        }

        if (!success)
            terrain_loader_unload(out_resource);

        return success;

    }

    void terrain_loader_unload(TerrainResourceData* resource)
    {
    }

}