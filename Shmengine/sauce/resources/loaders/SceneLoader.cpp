#include "SceneLoader.hpp"

#include "systems/ResourceSystem.hpp"
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

    static const char* loader_type_path = "scenes/";

    bool32 scene_loader_load(const char* name, void* params, SceneResourceData* out_resource)
    {

        const char* format = "%s%s%s%s";
        char full_filepath[MAX_FILEPATH_LENGTH];

        CString::safe_print_s<const char*, const char*, const char*, const char*>
            (full_filepath, MAX_FILEPATH_LENGTH, format, get_base_path(), loader_type_path, name, ".shmene");

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("material_loader_load - Failed to open file for loading scene '%s'", full_filepath);
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
            SCENE,
            SKYBOX,
            MESH,
            PRIMITIVE_CUBE,
            DIRECTIONAL_LIGHT,
            POINT_LIGHT,
            TERRAIN
        };

        ParserScope scope = ParserScope::SCENE;
        // Read each line of the file.
        String line(512);
        uint64 line_length = 0;       

        String var_name;
        String value;

        bool32 success = true;

        uint32 dir_lights_count = 0;
        uint32 point_lights_count = 0;
        uint32 meshes_count = 0;
        uint32 skyboxes_count = 0;
        uint32 terrains_count = 0;

        Math::Vec3f cube_dim = {};
        Math::Vec2f cube_tiling = {};
        String cube_material_name;

        uint32 line_number = 1;
        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content.c_str(), line, &continue_ptr))
        {
            line.trim();
            line_length = line.len();
            if (line_length < 1 || line[0] == '#')
            {
                line_number++;
                continue;
            }

            if (line[0] == '[')
            {
                if (line.equal_i("[Skybox]"))
                    skyboxes_count++;
                else if (line.equal_i("[Mesh]"))
                    meshes_count++;
                else if (line.equal_i("[PrimitiveCube]"))
                    meshes_count++;
                else if (line.equal_i("[DirectionalLight]"))
                    dir_lights_count++;
                else if (line.equal_i("[PointLight]"))
                    point_lights_count++;
                else if (line.equal_i("[Terrain]"))
                    terrains_count++;
            }
        }

        out_resource->skyboxes.init(skyboxes_count, 0, AllocationTag::RESOURCE);
        out_resource->meshes.init(meshes_count, 0, AllocationTag::RESOURCE);
        out_resource->terrains.init(terrains_count, 0, AllocationTag::RESOURCE);
        out_resource->dir_lights.init(dir_lights_count, 0, AllocationTag::RESOURCE);
        out_resource->point_lights.init(point_lights_count, 0, AllocationTag::RESOURCE);

        uint32 dir_light_i = INVALID_ID;
        uint32 point_light_i = INVALID_ID;
        uint32 mesh_i = INVALID_ID;
        uint32 skybox_i = INVALID_ID;
        uint32 terrain_i = INVALID_ID;

        out_resource->transform = Math::transform_create();

        line_number = 1;
        continue_ptr = 0;
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
            if (line[0] == '[')
            {
                if (scope == ParserScope::SCENE)
                {
                    if (line.equal_i("[Skybox]"))
                    {
                        scope = ParserScope::SKYBOX;
                        skybox_i++;
                    }                        
                    else if (line.equal_i("[Mesh]"))
                    {
                        scope = ParserScope::MESH;
                        mesh_i++;
                        out_resource->meshes[mesh_i].transform = Math::transform_create();
                    }
                    else if (line.equal_i("[PrimitiveCube]"))
                    {
                        scope = ParserScope::PRIMITIVE_CUBE;
                        cube_dim = {};
                        cube_tiling = {};
                        cube_material_name = "";
                        mesh_i++;
                        out_resource->meshes[mesh_i].transform = Math::transform_create();
                    }
                    else if (line.equal_i("[DirectionalLight]"))
                    {
                        scope = ParserScope::DIRECTIONAL_LIGHT;
                        dir_light_i++;

                    }
                    else if (line.equal_i("[PointLight]"))
                    {
                        scope = ParserScope::POINT_LIGHT;
                        point_light_i++;
                    }
                    else if (line.equal_i("[Terrain]"))
                    {
                        scope = ParserScope::TERRAIN;
                        terrain_i++;
                        out_resource->terrains[terrain_i].xform = Math::transform_create();
                    }
                    else
                    {
                        SHMERRORV("There is an error in scene scope syntax on line %u", line_number);
                        success = false;
                        break;
                    }
                }
                else
                {
                    if (line.equal_i("[/]"))
                    {
                        if (scope == ParserScope::PRIMITIVE_CUBE)
                        {
                            SceneMeshResourceData* cube_mesh = &out_resource->meshes[mesh_i];
                            if (!cube_material_name.is_empty() && !cube_mesh->name.is_empty())
                            {
                                cube_mesh->g_configs.init(1, 0);
                                cube_mesh->g_configs.emplace();
                                Renderer::generate_cube_config(cube_dim.x, cube_dim.y, cube_dim.z, cube_tiling.x, cube_tiling.y, cube_mesh->name.c_str(), cube_mesh->g_configs[0].data_config);
                                CString::copy(cube_material_name.c_str(), cube_mesh->g_configs[0].material_name, max_material_name_length);
                            }
                            else
                            {
                                SHMERRORV("Could not create geometry config for cube primitive due to missing data.");
                                success = false;
                            }
                        }

                        scope = ParserScope::SCENE;
                    }                     
                    else
                    {
                        SHMERRORV("There is an error in scene scope syntax on line %u", line_number);
                        success = false;
                        break;
                    }
                }

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

            if (scope == ParserScope::SCENE)
            {
                // Process the variable.
                if (var_name.equal_i("version"))
                {
                    // TODO: version
                }
                else if (var_name.equal_i("name"))
                {
                    out_resource->name = value;
                }
                else if (var_name.equal_i("description"))
                {
                    out_resource->description = value;
                }
                else if (var_name.equal_i("max_meshes_count"))
                {
                    if (!CString::parse_u32(value.c_str(), out_resource->max_meshes_count))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                }
                else if (var_name.equal_i("max_terrains_count"))
                {
                    if (!CString::parse_u32(value.c_str(), out_resource->max_terrains_count))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                }
                else if (var_name.equal_i("max_p_lights_count"))
                {
                    if (!CString::parse_u32(value.c_str(), out_resource->max_p_lights_count))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                }
            }
            else if (scope == ParserScope::SKYBOX)
            {
                SceneSkyboxResourceData* skybox = &out_resource->skyboxes[skybox_i];

                if (var_name.equal_i("name"))
                {
                    skybox->name = value;
                }
                else if (var_name.equal_i("cubemap_name"))
                {
                    skybox->cubemap_name = value;
                }
            }
            else if (scope == ParserScope::MESH || scope == ParserScope::PRIMITIVE_CUBE)
            {
                SceneMeshResourceData* mesh = &out_resource->meshes[mesh_i];

                if (var_name.equal_i("name"))
                {
                    mesh->name = value;
                }
                else if (var_name.equal_i("parent_name"))
                {
                    mesh->parent_name = value;
                }
                else if (var_name.equal_i("position"))
                {
                    Math::Vec3f position;
                    if (!CString::parse(value.c_str(), position))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                    Math::transform_translate(mesh->transform, position);
                }
                else if (var_name.equal_i("rotation"))
                {
                    Math::Vec4f rotation;
                    if (!CString::parse(value.c_str(), rotation))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                    Math::transform_rotate(mesh->transform, rotation);
                }
                else if (var_name.equal_i("scale"))
                {
                    Math::Vec3f scalar;
                    if (!CString::parse(value.c_str(), scalar))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                    Math::transform_scale(mesh->transform, scalar);
                }

                if (scope == ParserScope::MESH)
                {
                    if (var_name.equal_i("resource_name"))
                    {
                        mesh->resource_name = value;
                    }
                }
                else
                {
                    if (var_name.equal_i("dim"))
                    {
                        if (!CString::parse(value.c_str(), cube_dim))
                        {
                            SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                            success = false;
                            continue;
                        }
                    }
                    if (var_name.equal_i("tiling"))
                    {
                        if (!CString::parse(value.c_str(), cube_tiling))
                        {
                            SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                            success = false;
                            continue;
                        }
                    }
                    else if (var_name.equal_i("material_name"))
                    {
                        cube_material_name = value;
                    }
                }
            }
            else if (scope == ParserScope::DIRECTIONAL_LIGHT)
            {
                DirectionalLight* dir_light = &out_resource->dir_lights[dir_light_i];

                if (var_name.equal_i("color"))
                {
                    if (!CString::parse(value.c_str(), dir_light->color))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                }
                else if (var_name.equal_i("direction"))
                {
                    if (!CString::parse(value.c_str(), dir_light->direction))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                }
            }
            else if (scope == ParserScope::POINT_LIGHT)
            {
                PointLight* point_light = &out_resource->point_lights[point_light_i];

                if (var_name.equal_i("color"))
                {
                    if (!CString::parse(value.c_str(), point_light->color))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                }
                else if (var_name.equal_i("position"))
                {
                    if (!CString::parse(value.c_str(), point_light->position))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                }
                else if (var_name.equal_i("constant_f"))
                {
                    if (!CString::parse_f32(value.c_str(), point_light->constant_f))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                }
                else if (var_name.equal_i("linear"))
                {
                    if (!CString::parse_f32(value.c_str(), point_light->linear))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                }
                else if (var_name.equal_i("quadratic"))
                {
                    if (!CString::parse_f32(value.c_str(), point_light->quadratic))
                    {
                        SHMERRORV("Failed parsing value for %s on line %u", value.c_str(), line_number);
                        success = false;
                        continue;
                    }
                }
            }    
            else if (scope == ParserScope::TERRAIN)
            {
                SceneTerrainResourceData* terrain = &out_resource->terrains[terrain_i];

                if (var_name.equal_i("name"))
                {
                    terrain->name = value;
                }
                else if (var_name.equal_i("resource_name"))
                {
                    terrain->resource_name = value;
                }
            }

            line_number++;
        }

        FileSystem::file_close(&f);

        if (out_resource->name.is_empty())
        {
            SHMERRORV("Unsufficient data describing scene in file %s.", name);
            success = false;
        }

        if (!success)
            scene_loader_unload(out_resource);

        return success;

    }

    void scene_loader_unload(SceneResourceData* resource)
    {
        for (uint32 i = 0; i < resource->skyboxes.capacity; i++)
        {
            resource->skyboxes[i].name.free_data();
            resource->skyboxes[i].cubemap_name.free_data();
        }

        for (uint32 i = 0; i < resource->meshes.capacity; i++)
        {
            resource->meshes[i].name.free_data();
            resource->meshes[i].resource_name.free_data();
            resource->meshes[i].parent_name.free_data();

            resource->meshes[i].g_configs.free_data();
        }

        for (uint32 i = 0; i < resource->terrains.capacity; i++)
        {
            resource->terrains[i].name.free_data();
            resource->terrains[i].resource_name.free_data();
        }

        resource->name.free_data();
        resource->description.free_data();
        resource->skyboxes.free_data();
        resource->point_lights.free_data();
        resource->dir_lights.free_data();
        resource->meshes.free_data();
    }

}