#include "ShaderLoader.hpp"

#include "resources/ResourceTypes.hpp"
#include "LoaderUtils.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"

namespace ResourceSystem
{
	static bool32 shader_loader_load(ResourceLoader* loader, const char* name, Resource* out_resource)
	{

        const char* format = "%s%s%s%s";
        char full_filepath[MAX_FILEPATH_LENGTH];

        CString::safe_print_s<const char*, const char*, const char*, const char*>
            (full_filepath, MAX_FILEPATH_LENGTH, format, get_base_path(), loader->type_path, name, ".shadercfg");

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("shader_resource_loader_create - Failed to open file for loading material '%s'", full_filepath);
            return false;
        }

        uint32 file_size = FileSystem::get_file_size32(&f);
        String file_content(file_size + 1);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(&f, file_content, &bytes_read))
        {
            SHMERRORV("material_loader_load - failed to read from file: '%s'.", full_filepath);
            return false;
        }

        out_resource->allocation_tag = AllocationTag::MAIN;
        ShaderConfig* r_data = (ShaderConfig*)Memory::allocate(sizeof(ShaderConfig), true, out_resource->allocation_tag);

        r_data->attributes.init(1, 0, AllocationTag::MAIN);
        r_data->uniforms.init(1, 0, AllocationTag::MAIN);
        r_data->stages.init(1, 0, AllocationTag::MAIN);
        r_data->use_instances = false;
        r_data->use_local = false;
        r_data->stage_names.init(1, 0, AllocationTag::MAIN);
        r_data->stage_filenames.init(1, 0, AllocationTag::MAIN);

        String var_name(32);
        String value(32);
        String line(512);
        uint32 line_number = 1;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content, line, &continue_ptr)) 
        {

            line.trim();

            if (line.len() < 1 || line[0] == '#') {
                line_number++;
                continue;
            }

            int32 equal_index = line.index_of('=');
            if (equal_index == -1) {
                SHMWARNV("Potential formatting issue found in file '%s': '=' token not found. Skipping line %u.", full_filepath, line_number);
                line_number++;
                continue;
            }

            var_name = mid(line, 0, equal_index);
            var_name.trim();

            value = mid(line, equal_index + 1);
            value.trim();

            if (var_name.equal_i("version")) {
                // TODO: version
            }
            else if (var_name.equal_i("name")) {
                r_data->name = value;
            }
            else if (var_name.equal_i("renderpass")) {
                r_data->renderpass_name = value;
            }
            else if (var_name.equal_i("stages")) {
                r_data->stage_names = value.split(',');
                for (uint32 i = 0; i < r_data->stage_names.count; i++)
                {
                    r_data->stage_names[i].trim();
                    if (r_data->stage_names[i].equal_i("frag") || r_data->stage_names[i].equal_i("fragment"))
                        r_data->stages.push(ShaderStage::FRAGMENT);
                    else if (r_data->stage_names[i].equal_i("vert") || r_data->stage_names[i].equal_i("vertex"))
                        r_data->stages.push(ShaderStage::VERTEX);
                    else if (r_data->stage_names[i].equal_i("geom") || r_data->stage_names[i].equal_i("geometry"))
                        r_data->stages.push(ShaderStage::GEOMETRY);
                    else if (r_data->stage_names[i].equal_i("comp") || r_data->stage_names[i].equal_i("compute"))
                        r_data->stages.push(ShaderStage::COMPUTE);
                    else
                        SHMERRORV("shader_loader_load - Invalid file layout. Unrecognized stage '%s'", r_data->stage_names[i].c_str());
                }
            }
            else if (var_name.equal_i("stagefiles")) {
                r_data->stage_filenames = value.split(',');
                for (uint32 i = 0; i < r_data->stage_filenames.count; i++)
                    r_data->stage_filenames[i].trim();
            }
            else if (var_name.equal_i("use_instance")) {
                CString::parse_b32(value.c_str(), r_data->use_instances);
            }
            else if (var_name.equal_i("use_local") || var_name.equal_i("use_locals")) {
                CString::parse_b32(value.c_str(), r_data->use_local);
            }
            else if (var_name.equal_i("attributes") || var_name.equal_i("attribute")) {
                Darray<String> tmp = value.split(',');               
                if (tmp.count != 2)
                    SHMERROR("shader_loader_load - Invalid file layout. Attribute fields must be 'type,name'. Skipping.");
                else
                {
                    tmp[0].trim();
                    tmp[1].trim();
                    ShaderAttributeConfig attribute;
                    // Parse field type
                    if (tmp[0].equal_i("float32")) {
                        attribute.type = ShaderAttributeType::FLOAT32;
                        attribute.size = 4;
                    }
                    else if (tmp[0].equal_i("vec2")) {
                        attribute.type = ShaderAttributeType::FLOAT32_2;
                        attribute.size = 8;
                    }
                    else if (tmp[0].equal_i("vec3")) {
                        attribute.type = ShaderAttributeType::FLOAT32_3;
                        attribute.size = 12;
                    }
                    else if (tmp[0].equal_i("vec4")) {
                        attribute.type = ShaderAttributeType::FLOAT32_4;
                        attribute.size = 16;
                    }
                    else if (tmp[0].equal_i("uint8")) {
                        attribute.type = ShaderAttributeType::UINT8;
                        attribute.size = 1;
                    }
                    else if (tmp[0].equal_i("uint16")) {
                        attribute.type = ShaderAttributeType::UINT16;
                        attribute.size = 2;
                    }
                    else if (tmp[0].equal_i("uint32")) {
                        attribute.type = ShaderAttributeType::UINT32;
                        attribute.size = 4;
                    }
                    else if (tmp[0].equal_i("int8")) {
                        attribute.type = ShaderAttributeType::INT8;
                        attribute.size = 1;
                    }
                    else if (tmp[0].equal_i("int16")) {
                        attribute.type = ShaderAttributeType::INT16;
                        attribute.size = 2;
                    }
                    else if (tmp[0].equal_i("int32")) {
                        attribute.type = ShaderAttributeType::INT32;
                        attribute.size = 4;
                    }
                    else {
                        SHMERROR("shader_loader_load - Invalid file layout. Attribute type must be float32, vec2, vec3, vec4, int8, int16, int32, uint8, uint16, or uint32.");
                        attribute.size = 0;
                    }

                    if (attribute.size)
                    {
                        attribute.name = tmp[1];

                        r_data->attributes.push(attribute);
                    }
                }
            }
            else if (var_name.equal_i("uniforms") || var_name.equal_i("uniform")) 
            {
                Darray<String> tmp = value.split(',');
                if (tmp.count != 3)
                    SHMERROR("shader_loader_load - Invalid file layout. Attribute fields must be 'type,name'. Skipping.");
                else
                {
                    tmp[0].trim();
                    tmp[1].trim();
                    tmp[2].trim();
                    ShaderUniformConfig uniform;
                    // Parse field type
                    if (tmp[0].equal_i("float32")) {
                        uniform.type = ShaderUniformType::FLOAT32;
                        uniform.size = 4;
                    }
                    else if (tmp[0].equal_i("vec2")) {
                        uniform.type = ShaderUniformType::FLOAT32_2;
                        uniform.size = 8;
                    }
                    else if (tmp[0].equal_i("vec3")) {
                        uniform.type = ShaderUniformType::FLOAT32_3;
                        uniform.size = 12;
                    }
                    else if (tmp[0].equal_i("vec4")) {
                        uniform.type = ShaderUniformType::FLOAT32_4;
                        uniform.size = 16;
                    }
                    else if (tmp[0].equal_i("uint8")) {
                        uniform.type = ShaderUniformType::UINT8;
                        uniform.size = 1;
                    }
                    else if (tmp[0].equal_i("uint16")) {
                        uniform.type = ShaderUniformType::UINT16;
                        uniform.size = 2;
                    }
                    else if (tmp[0].equal_i("uint32")) {
                        uniform.type = ShaderUniformType::UINT32;
                        uniform.size = 4;
                    }
                    else if (tmp[0].equal_i("int8")) {
                        uniform.type = ShaderUniformType::INT8;
                        uniform.size = 1;
                    }
                    else if (tmp[0].equal_i("int16")) {
                        uniform.type = ShaderUniformType::INT16;
                        uniform.size = 2;
                    }
                    else if (tmp[0].equal_i("int32")) {
                        uniform.type = ShaderUniformType::INT32;
                        uniform.size = 4;
                    }
                    else if (tmp[0].equal_i("mat4")) {
                        uniform.type = ShaderUniformType::MAT4;
                        uniform.size = 64;
                    }
                    else if (tmp[0].equal_i("samp")) {
                        uniform.type = ShaderUniformType::SAMPLER;
                        uniform.size = 1;
                    }
                    else {
                        SHMERROR("shader_loader_load - Invalid file layout. Uniform type must be float32, vec2, vec3, vec4, int8, int16, int32, uint8, uint16, uint32, mat4 or samp.");
                        uniform.size = 0;
                    }

                    if (uniform.size)
                    {

                        if (tmp[1].equal_i("0") || tmp[1].equal_i("global"))
                        {
                            uniform.scope = ShaderScope::GLOBAL;
                        }
                        else if (tmp[1].equal_i("1") || tmp[1].equal_i("instance"))
                        {
                            uniform.scope = ShaderScope::INSTANCE;
                        }
                        else if (tmp[1].equal_i("2") || tmp[1].equal_i("local"))
                        {
                            uniform.scope = ShaderScope::LOCAL;
                        }
                        else
                        {
                            SHMERROR("shader_loader_load - Invalid file layout: Uniform scope must be 0 for global, 1 for instance or 2 for local.");
                            SHMERROR("Defaulting to global.");
                            uniform.scope = ShaderScope::GLOBAL;
                        }

                        if (uniform.type == ShaderUniformType::SAMPLER)
                            uniform.size = 0;
                        uniform.name = tmp[2];

                        r_data->uniforms.push(uniform);
                    }
                }
            }

            line_number++;

        }

        FileSystem::file_close(&f);

        out_resource->full_path = full_filepath;
        out_resource->data = r_data;
        out_resource->data_size = sizeof(ShaderConfig);

        return true;

	}

    static void shader_loader_unload(ResourceLoader* self, Resource* resource) 
    {

        if (resource->data)
        {
            ShaderConfig* data = (ShaderConfig*)resource->data;
            (*data).~ShaderConfig();
        }

        resource_unload(self, resource);

    }

    ResourceLoader shader_resource_loader_create()
    {
        ResourceLoader loader;
        loader.type = ResourceType::SHADER;
        loader.custom_type = 0;
        loader.load = shader_loader_load;
        loader.unload = shader_loader_unload;
        loader.type_path = "shaders/";

        return loader;
    }
}