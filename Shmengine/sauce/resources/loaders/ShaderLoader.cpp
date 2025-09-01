#include "ShaderLoader.hpp"

#include "core/Engine.hpp"
#include "renderer/RendererTypes.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"

namespace ResourceSystem
{

    static const char* loader_type_path = "shaders/configs/";

	bool8 shader_loader_load(const char* name, ShaderResourceData* out_resource)
	{

        using namespace Renderer;
        const char* format = "%s%s%s%s";
        char full_filepath[Constants::max_filepath_length];

        CString::safe_print_s<const char*, const char*, const char*, const char*>
            (full_filepath, Constants::max_filepath_length, format, Engine::get_assets_base_path(), loader_type_path, name, ".shadercfg");

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

        out_resource->cull_mode = RenderCullMode::Back;
        out_resource->topologies = RenderTopologyTypeFlags::TriangleList;
        out_resource->attributes.init(1, 0);
        out_resource->uniforms.init(1, 0);
        out_resource->stages.init(1, 0);

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
                CString::copy(value.c_str(), out_resource->name, Constants::max_shader_name_length) ;
            }
            else if (var_name.equal_i("renderpass")) 
            {
                //out_config->renderpass_name = value;
            }
            else if (var_name.equal_i("depth_test")) 
            {
                 CString::parse(value.c_str(), &out_resource->depth_test);
            }
            else if (var_name.equal_i("depth_write")) 
            {
                CString::parse(value.c_str(), &out_resource->depth_write);
            }
            else if (var_name.equal_i("stages")) 
            {
                Darray<String> stage_names;
                value.split(stage_names, ',');

                if (!out_resource->stages.capacity)
                    out_resource->stages.init(stage_names.count, 0, AllocationTag::Resource);
                else if (stage_names.count > out_resource->stages.capacity)
                    out_resource->stages.resize(stage_names.count);
                
                if (stage_names.count > out_resource->stages.count)
                    out_resource->stages.set_count(stage_names.count);

                for (uint32 i = 0; i < stage_names.count; i++)
                {
                    stage_names[i].trim();
                    if (stage_names[i].equal_i("frag") || stage_names[i].equal_i("fragment"))
                        out_resource->stages[i].stage = ShaderStage::Fragment;
                    else if (stage_names[i].equal_i("vert") || stage_names[i].equal_i("vertex"))
                        out_resource->stages[i].stage = ShaderStage::Vertex;
                    else if (stage_names[i].equal_i("geom") || stage_names[i].equal_i("geometry"))
                        out_resource->stages[i].stage = ShaderStage::Geometry;
                    else if (stage_names[i].equal_i("comp") || stage_names[i].equal_i("compute"))
                        out_resource->stages[i].stage = ShaderStage::Compute;
                    else
                        SHMERRORV("shader_loader_load - Invalid file layout. Unrecognized stage '%s'", stage_names[i].c_str());
                }
            }
            else if (var_name.equal_i("stagefiles")) 
            {
                Darray<String> stage_filenames;
                value.split(stage_filenames, ',');

                if (!out_resource->stages.capacity)
                    out_resource->stages.init(stage_filenames.count, 0, AllocationTag::Resource);
                else if (stage_filenames.count > out_resource->stages.capacity)
                    out_resource->stages.resize(stage_filenames.count);

                if (stage_filenames.count > out_resource->stages.count)
                    out_resource->stages.set_count(stage_filenames.count);

                for (uint32 i = 0; i < stage_filenames.count; i++)
                    CString::copy(stage_filenames[i].c_str(), out_resource->stages[i].filename, Constants::max_filename_length);

                for (uint32 i = 0; i < stage_filenames.count; i++)
                    stage_filenames[i].free_data();
                stage_filenames.free_data();
            }
            else if (var_name.equal_i("topology"))
            {
                Darray<String> topologies;
                value.split(topologies, ',');

                out_resource->topologies = RenderTopologyTypeFlags::None;

                for (uint32 i = 0; i < topologies.count; i++)
                {
                    topologies[i].trim();
                    if (topologies[i].equal_i("triangle_list"))
                        out_resource->topologies |= RenderTopologyTypeFlags::TriangleList;
                    else if (topologies[i].equal_i("triangle_strip"))
                        out_resource->topologies |= RenderTopologyTypeFlags::TriangleStrip;
                    else if (topologies[i].equal_i("triangle_fan"))
                        out_resource->topologies |= RenderTopologyTypeFlags::TriangleFan;
                    else if (topologies[i].equal_i("line_list"))
                        out_resource->topologies |= RenderTopologyTypeFlags::LineList;
                    else if (topologies[i].equal_i("line_strip"))
                        out_resource->topologies |= RenderTopologyTypeFlags::LineStrip;
                    else if (topologies[i].equal_i("point_list"))
                        out_resource->topologies |= RenderTopologyTypeFlags::PointList;
                    else
                        SHMERRORV("Invalid file layout. Unrecognized topology '%s'", topologies[i].c_str());
                }

                for (uint32 i = 0; i < topologies.count; i++)
                    topologies[i].free_data();
                topologies.free_data();
            }
            else if (var_name.equal_i("cull_mode")) 
            {
                if (value.equal_i("front"))
                    out_resource->cull_mode = RenderCullMode::Front;
                else if (value.equal_i("back"))
                    out_resource->cull_mode = RenderCullMode::Back;
                else if (value.equal_i("both"))
                    out_resource->cull_mode = RenderCullMode::Both;
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
                    if (tmp[0].equal_i("float32")) 
                    {
                        attribute.type = ShaderAttributeType::Float32;
                        attribute.size = 4;
                    }
                    else if (tmp[0].equal_i("vec2")) 
                    {
                        attribute.type = ShaderAttributeType::Float32_2;
                        attribute.size = 8;
                    }
                    else if (tmp[0].equal_i("vec3")) 
                    {
                        attribute.type = ShaderAttributeType::Float32_3;
                        attribute.size = 12;
                    }
                    else if (tmp[0].equal_i("vec4")) 
                    {
                        attribute.type = ShaderAttributeType::Float32_4;
                        attribute.size = 16;
                    }
                    else if (tmp[0].equal_i("uint8")) 
                    {
                        attribute.type = ShaderAttributeType::UInt8;
                        attribute.size = 1;
                    }
                    else if (tmp[0].equal_i("uint16")) 
                    {
                        attribute.type = ShaderAttributeType::UInt16;
                        attribute.size = 2;
                    }
                    else if (tmp[0].equal_i("uint32")) 
                    {
                        attribute.type = ShaderAttributeType::UInt32;
                        attribute.size = 4;
                    }
                    else if (tmp[0].equal_i("int8")) 
                    {
                        attribute.type = ShaderAttributeType::Int8;
                        attribute.size = 1;
                    }
                    else if (tmp[0].equal_i("int16")) 
                    {
                        attribute.type = ShaderAttributeType::Int16;
                        attribute.size = 2;
                    }
                    else if (tmp[0].equal_i("int32")) 
                    {
                        attribute.type = ShaderAttributeType::Int32;
                        attribute.size = 4;
                    }
                    else 
                    {
                        SHMERROR("shader_loader_load - Invalid file layout. Attribute type must be float32, vec2, vec3, vec4, int8, int16, int32, uint8, uint16, or uint32.");
                        attribute.size = 0;
                    }

                    if (attribute.size)
                    {
                        CString::copy(tmp[1].c_str(), attribute.name, Constants::max_shader_attribute_name_length);
                        out_resource->attributes.emplace(attribute);
                    }

                    for (uint32 i = 0; i < tmp.count; i++)
                        tmp[i].free_data();
                    tmp.free_data();
                }
            }
            else if (var_name.equal_i("uniforms") || var_name.equal_i("uniform")) 
            {
                Darray<String> tmp(3, 0);
                value.split(tmp, ',');
                if (tmp.count != 3)
                {
                    SHMERROR("shader_loader_load - Invalid file layout. Attribute fields must be 'type,name'. Skipping.");
                    continue;
                }
                    
                else
                {
                    tmp[0].trim();
                    tmp[1].trim();
                    tmp[2].trim();
                    ShaderUniformConfig uniform = {};
                    // Parse field type
                    if (tmp[0].equal_i("float32")) 
                    {
                        uniform.type = ShaderUniformType::Float32;
                        uniform.size = 4;
                    }
                    else if (tmp[0].equal_i("vec2")) 
                    {
                        uniform.type = ShaderUniformType::Float32_2;
                        uniform.size = 8;
                    }
                    else if (tmp[0].equal_i("vec3")) 
                    {
                        uniform.type = ShaderUniformType::Float32_3;
                        uniform.size = 12;
                    }
                    else if (tmp[0].equal_i("vec4")) 
                    {
                        uniform.type = ShaderUniformType::Float32_4;
                        uniform.size = 16;
                    }
                    else if (tmp[0].equal_i("uint8")) 
                    {
                        uniform.type = ShaderUniformType::UInt8;
                        uniform.size = 1;
                    }
                    else if (tmp[0].equal_i("uint16")) 
                    {
                        uniform.type = ShaderUniformType::UInt16;
                        uniform.size = 2;
                    }
                    else if (tmp[0].equal_i("uint32")) 
                    {
                        uniform.type = ShaderUniformType::UInt32;
                        uniform.size = 4;
                    }
                    else if (tmp[0].equal_i("int8")) 
                    {
                        uniform.type = ShaderUniformType::Int8;
                        uniform.size = 1;
                    }
                    else if (tmp[0].equal_i("int16")) 
                    {
                        uniform.type = ShaderUniformType::Int16;
                        uniform.size = 2;
                    }
                    else if (tmp[0].equal_i("int32")) 
                    {
                        uniform.type = ShaderUniformType::Int32;
                        uniform.size = 4;
                    }
                    else if (tmp[0].equal_i("mat4")) 
                    {
                        uniform.type = ShaderUniformType::Mat4;
                        uniform.size = 64;
                    }
                    else if (tmp[0].equal_i("samp")) 
                    {
                        uniform.type = ShaderUniformType::Sampler;
                        uniform.size = 1;
                    }
                    else if (tmp[0].nequal_i("struct", 6)) 
                    {
                        if (tmp[0].len() <= 6)
                        {
                            SHMERRORV("Failed to load struct uniform. Size missing.", tmp[0].c_str());
                            return false;
                        }
                        uint16 size = 0;
                        if (!CString::parse(&tmp[0][6], &size))
                        {
                            SHMERRORV("Failed to parse uniform struct size: '%s'", tmp[0].c_str());
                            return false;
                        }
                        uniform.type = ShaderUniformType::Custom;
                        uniform.size = size;
                    }
                    else 
                    {
                        SHMERROR("shader_loader_load - Invalid file layout. Uniform type must be float32, vec2, vec3, vec4, int8, int16, int32, uint8, uint16, uint32, mat4 or samp.");
                        uniform.size = 0;
                    }

                    if (uniform.size)
                    {

                        if (tmp[1].equal_i("0") || tmp[1].equal_i("global"))
                        {
                            uniform.scope = ShaderScope::Global;
                        }
                        else if (tmp[1].equal_i("1") || tmp[1].equal_i("instance"))
                        {
                            uniform.scope = ShaderScope::Instance;
                        }
                        else if (tmp[1].equal_i("2") || tmp[1].equal_i("local"))
                        {
                            uniform.scope = ShaderScope::Local;
                        }
                        else
                        {
                            SHMERROR("shader_loader_load - Invalid file layout: Uniform scope must be 0 for global, 1 for instance or 2 for local.");
                            SHMERROR("Defaulting to global.");
                            uniform.scope = ShaderScope::Global;
                        }

                        if (uniform.type == ShaderUniformType::Sampler)
                            uniform.size = 0;

                        CString::copy(tmp[2].c_str(), uniform.name, Constants::max_shader_uniform_name_length);
                        out_resource->uniforms.emplace(uniform);
                    }
                }

                for (uint32 i = 0; i < tmp.count; i++)
                    tmp[i].free_data();
                tmp.free_data();
            }

            line_number++;

        }

        FileSystem::file_close(&f);

        return true;

	}

    void shader_loader_unload(ShaderResourceData* resource)
    {
        resource->stages.free_data();
        resource->attributes.free_data();
        resource->uniforms.free_data();
    }

    ShaderConfig shader_loader_get_config_from_resource(ShaderResourceData* resource, Renderer::RenderPass* renderpass)
    {
        ShaderConfig config = {};
        config.name = resource->name;
        config.renderpass = renderpass;
        config.cull_mode = resource->cull_mode;
        config.topologies = resource->topologies;
        config.depth_test = resource->depth_test;
        config.depth_write = resource->depth_write;

        config.stages = resource->stages.data;
        config.stages_count = resource->stages.count;
        config.attributes = resource->attributes.data;
        config.attributes_count = resource->attributes.count;
        config.uniforms = resource->uniforms.data;
        config.uniforms_count = resource->uniforms.count;

        return config;
    }

}