#include "ShaderLoader.hpp"

#include "resources/ResourceTypes.hpp"
#include "renderer/RendererTypes.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"

namespace ResourceSystem
{

    static const char* loader_type_path = "shaders/configs/";

	bool32 shader_loader_load(const char* name, void* params, Renderer::ShaderConfig* out_config)
	{

        using namespace Renderer;
        const char* format = "%s%s%s%s";
        char full_filepath[MAX_FILEPATH_LENGTH];

        CString::safe_print_s<const char*, const char*, const char*, const char*>
            (full_filepath, MAX_FILEPATH_LENGTH, format, get_base_path(), loader_type_path, name, ".shadercfg");

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("Failed to open file for loading material '%s'", full_filepath);
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

        out_config->cull_mode = ShaderFaceCullMode::BACK;
        out_config->attributes.init(1, 0);
        out_config->uniforms.init(1, 0);
        out_config->stages.init(1, 0);
        out_config->stage_names.init(1, 0);
        out_config->stage_filenames.init(1, 0);

        String var_name(512);
        String value(512);
        String line(512);
        uint32 line_number = 1;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content.c_str(), line, &continue_ptr))
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

            mid(var_name, line.c_str(), 0, equal_index);
            var_name.trim();

            mid(value, line.c_str(), equal_index + 1);
            value.trim();

            if (var_name.equal_i("version")) 
            {
                // TODO: version
            }
            else if (var_name.equal_i("name")) 
            {
                out_config->name = value;
            }
            else if (var_name.equal_i("renderpass")) 
            {
                //out_config->renderpass_name = value;
            }
            else if (var_name.equal_i("depth_test")) 
            {
                 CString::parse_b8(value.c_str(), out_config->depth_test);
            }
            else if (var_name.equal_i("depth_write")) 
            {
                CString::parse_b8(value.c_str(), out_config->depth_write);
            }
            else if (var_name.equal_i("stages")) 
            {
                value.split(out_config->stage_names, ',');
                for (uint32 i = 0; i < out_config->stage_names.count; i++)
                {
                    out_config->stage_names[i].trim();
                    if (out_config->stage_names[i].equal_i("frag") || out_config->stage_names[i].equal_i("fragment"))
                        out_config->stages.push(ShaderStage::FRAGMENT);
                    else if (out_config->stage_names[i].equal_i("vert") || out_config->stage_names[i].equal_i("vertex"))
                        out_config->stages.push(ShaderStage::VERTEX);
                    else if (out_config->stage_names[i].equal_i("geom") || out_config->stage_names[i].equal_i("geometry"))
                        out_config->stages.push(ShaderStage::GEOMETRY);
                    else if (out_config->stage_names[i].equal_i("comp") || out_config->stage_names[i].equal_i("compute"))
                        out_config->stages.push(ShaderStage::COMPUTE);
                    else
                        SHMERRORV("shader_loader_load - Invalid file layout. Unrecognized stage '%s'", out_config->stage_names[i].c_str());
                }
            }
            else if (var_name.equal_i("stagefiles")) 
            {
                value.split(out_config->stage_filenames, ',');
                for (uint32 i = 0; i < out_config->stage_filenames.count; i++)
                    out_config->stage_filenames[i].trim();
            }
            else if (var_name.equal_i("cull_mode")) 
            {
                if (value.equal_i("front"))
                    out_config->cull_mode = ShaderFaceCullMode::FRONT;
                else if (value.equal_i("back"))
                    out_config->cull_mode = ShaderFaceCullMode::BACK;
                else if (value.equal_i("both"))
                    out_config->cull_mode = ShaderFaceCullMode::BOTH;
            }
            else if (var_name.equal_i("attributes") || var_name.equal_i("attribute")) 
            {
                Darray<String> tmp(2, 0);
                value.split(tmp, ',');
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

                        out_config->attributes.push_steal(attribute);
                    }
                }
            }
            else if (var_name.equal_i("uniforms") || var_name.equal_i("uniform")) 
            {
                Darray<String> tmp(3, 0);
                value.split(tmp, ',');
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

                        out_config->uniforms.push_steal(uniform);
                    }
                }
            }

            line_number++;

        }

        FileSystem::file_close(&f);

        return true;

	}

    void shader_loader_unload(Renderer::ShaderConfig* config)
    {

        for (uint32 i = 0; i < config->attributes.count; i++)
        {
            Renderer::ShaderAttributeConfig* att = &config->attributes[i];
            att->name.free_data();
        }
        config->attributes.free_data();
        
        for (uint32 i = 0; i < config->stage_filenames.count; i++)
        {
            String* stage_filename = &config->stage_filenames[i];
            stage_filename->free_data();
        }
        config->stage_filenames.free_data();

        for (uint32 i = 0; i < config->stage_names.count; i++)
        {
            String* stage_name = &config->stage_names[i];
            stage_name->free_data();
        }
        config->stage_names.free_data();

        for (uint32 i = 0; i < config->uniforms.count; i++)
        {
            Renderer::ShaderUniformConfig* u = &config->uniforms[i];
            u->name.free_data();
        }
        config->uniforms.free_data();

        config->name.free_data();
        config->stages.free_data();

    }

}