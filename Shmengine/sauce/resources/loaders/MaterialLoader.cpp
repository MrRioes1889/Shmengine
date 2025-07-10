#include "MaterialLoader.hpp"

#include "core/Engine.hpp"
#include "systems/MaterialSystem.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"
#include "containers/Sarray.hpp"
#include "renderer/RendererTypes.hpp"

namespace ResourceSystem
{

    static const char* loader_type_path = "materials/";

    static bool32 write_shmt_file(MaterialResourceData* resource);

    static MaterialType parse_material_type(String* s, uint32 line_number)
    {
        if (s->equal_i("phong"))
        {
            return MaterialType::PHONG;
        }
        if (s->equal_i("pbr"))
        {
            SHMWARNV("PBR material type not upported yet. Setting to default phong.", line_number);
            return MaterialType::PHONG;
        }
        if (s->equal_i("ui"))
        {
            return MaterialType::UI;
        }
        if (s->equal_i("custom"))
        {
            return MaterialType::CUSTOM;
        }
        else
        {
            SHMWARNV("Failed to parse material type on line %u. Setting to default phong.", line_number);
            return MaterialType::PHONG;
        }
    }

    static TextureFilter::Value parse_texture_filter(String* s, uint32 line_number)
    {
        if (s->equal_i("nearest"))
        {
            return TextureFilter::NEAREST;
        }
        else if (s->equal_i("linear"))
        {
            return TextureFilter::LINEAR;
        }
        else
        {
            SHMWARNV("Failed to parse texture filter on line %u. Setting to default linear.", line_number);
            return TextureFilter::LINEAR;
        }
    }

    static TextureRepeat::Value parse_texture_repeat(String* s, uint32 line_number)
    {
        if (s->equal_i("repeat"))
        {
            return TextureRepeat::REPEAT;
        }
        else if (s->equal_i("mirrored_repeat"))
        {
            return TextureRepeat::MIRRORED_REPEAT;
        }
        else if (s->equal_i("clamp_to_edge"))
        {
            return TextureRepeat::CLAMP_TO_EDGE;
        }
        else if (s->equal_i("clamp_to_border"))
        {
            return TextureRepeat::CLAMP_TO_BORDER;
        }
        else
        {
            SHMWARNV("Failed to parse texture repeat on line %u. Setting to default repeat.", line_number);
            return TextureRepeat::REPEAT;
        }
    }

#define PARSE_PROPERTY_VALUE(s, v) if (!CString::parse(s, v)) \
    { \
        SHMERRORV("Failed parsing value on line %u", line_number); \
        success = false; \
    }

    static bool32 parse_property(String* s, uint32 line_number, MaterialProperty* out_property)
    {

        out_property->type = MaterialPropertyType::INVALID;

        int32 div_index = s->index_of('/');
        if (div_index <= 0)
        {
            SHMWARNV("Failed to parse property on line %u. Values have to be formatted as TYPE/VALUE.", line_number);
            return false;
        }

        if ((*s)[0] == 'u')
        {
            if (s->nequal_i("u8", div_index))
                out_property->type = MaterialPropertyType::UINT8;
            else if (s->nequal_i("u16", div_index))
                out_property->type = MaterialPropertyType::UINT16;
            else if (s->nequal_i("u32", div_index))
                out_property->type = MaterialPropertyType::UINT32;
            else if (s->nequal_i("u64", div_index))
                out_property->type = MaterialPropertyType::UINT64;
        }
        else if ((*s)[0] == 'i')
        {
            if (s->nequal_i("i8", div_index))
                out_property->type = MaterialPropertyType::INT8;
            else if (s->nequal_i("i16", div_index))
                out_property->type = MaterialPropertyType::INT16;
            else if (s->nequal_i("i32", div_index))
                out_property->type = MaterialPropertyType::INT32;
            else if (s->nequal_i("i64", div_index))
                out_property->type = MaterialPropertyType::INT64;
        }
        else if ((*s)[0] == 'f')
        {
            if (s->nequal_i("f32", div_index))
                out_property->type = MaterialPropertyType::FLOAT32;
            else if (s->nequal_i("f64", div_index))
                out_property->type = MaterialPropertyType::FLOAT64;
        }
        else
        {
            if (s->nequal_i("vec2", div_index))
                out_property->type = MaterialPropertyType::FLOAT32_2;
            else if (s->nequal_i("vec3", div_index))
                out_property->type = MaterialPropertyType::FLOAT32_3;
            else if (s->nequal_i("vec4", div_index))
                out_property->type = MaterialPropertyType::FLOAT32_4;
            else if (s->nequal_i("mat4", div_index))
                out_property->type = MaterialPropertyType::FLOAT32_16;
        }

        if (out_property->type == MaterialPropertyType::INVALID)
        {
            SHMWARNV("Failed to parse property on line %u. Unknown data type.", line_number);
            return false;
        }

        bool32 success = true;

        switch (out_property->type)
        {
        case MaterialPropertyType::UINT8:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index+1], &out_property->u8[0]);
            break;
        }
        case MaterialPropertyType::UINT16:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], &out_property->u16[0]);
            break;
        }
        case MaterialPropertyType::UINT32:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], &out_property->u32[0]);
            break;
        }
        case MaterialPropertyType::UINT64:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], &out_property->u64[0]);
            break;
        }
        case MaterialPropertyType::INT8:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], &out_property->i8[0]);
            break;
        }
        case MaterialPropertyType::INT16:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], &out_property->i16[0]);
            break;
        }
        case MaterialPropertyType::INT32:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], &out_property->i32[0]);
            break;
        }
        case MaterialPropertyType::INT64:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], &out_property->i64[0]);
            break;
        }
        case MaterialPropertyType::FLOAT32:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], &out_property->f32[0]);
            break;
        }
        case MaterialPropertyType::FLOAT64:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], &out_property->f64[0]);
            break;
        }
        case MaterialPropertyType::FLOAT32_2:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], (Math::Vec2f*)out_property->f32);
            break;
        }
        case MaterialPropertyType::FLOAT32_3:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], (Math::Vec3f*)out_property->f32);
            break;
        }
        case MaterialPropertyType::FLOAT32_4:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], (Math::Vec4f*)out_property->f32);
            break;
        }
        case MaterialPropertyType::FLOAT32_16:
        {
            PARSE_PROPERTY_VALUE(&(*s)[div_index + 1], (Math::Mat4*)out_property->f32);
            break;
        }
        }

        return success;

    }

	bool32 material_loader_load(const char* name, void* params, MaterialResourceData* out_resource)
	{

		const char* format = "%s%s%s%s";
		char full_filepath[Constants::max_filepath_length];

		CString::safe_print_s<const char*, const char*, const char*, const char*>
			(full_filepath, Constants::max_filepath_length, format, Engine::get_assets_base_path(), loader_type_path, name, ".shmt");

		FileSystem::FileHandle f;
		if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
		{
			SHMERRORV("material_loader_load - Failed to open file for loading material '%s'", full_filepath);
			return false;
		}

        enum class ParserScope
        {
            MATERIAL,
            PROPERTIES,
            TEXTURE_MAP
        };

        ParserScope scope = ParserScope::MATERIAL;

        out_resource->auto_release = true;
        CString::copy(Renderer::RendererConfig::builtin_shader_name_material_phong, out_resource->shader_name, Constants::max_shader_name_length);
		CString::copy(name, out_resource->name, Constants::max_material_name_length);

        out_resource->type = MaterialType::PHONG;
        out_resource->maps.init(3, 0, AllocationTag::RESOURCE);
        out_resource->properties.init(10, 0, AllocationTag::RESOURCE);

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

        uint32 property_i = Constants::max_u32;
        uint32 texture_map_i = Constants::max_u32;

        bool32 success = true;

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

            if (line[0] == '[')
            {
                if (scope == ParserScope::MATERIAL)
                {
                    if (line.equal_i("[Properties]"))
                    {
                        scope = ParserScope::PROPERTIES;
                    }
                    else if (line.equal_i("[TextureMap]"))
                    {
                        scope = ParserScope::TEXTURE_MAP;
                        out_resource->maps.emplace();
                        texture_map_i = out_resource->maps.count - 1;
                        TextureMapResourceData* map = &out_resource->maps[texture_map_i];
                        map->filter_mag = map->filter_min = TextureFilter::LINEAR;
                        map->repeat_u = map->repeat_v = map->repeat_w = TextureRepeat::REPEAT;
                    }
                    else
                    {
                        SHMERRORV("There is an error in material scope syntax on line %u", line_number);
                        success = false;
                        break;
                    }
                }
                else
                {
                    if (line.equal_i("[/]"))
                    {
                        scope = ParserScope::MATERIAL;
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

            if (scope == ParserScope::MATERIAL)
            {
                if (var_name.equal_i("name"))
                {
                    CString::copy(value.c_str(), out_resource->name, Constants::max_material_name_length);
                }
                else if (var_name.equal_i("type"))
                {
                    out_resource->type = parse_material_type(&value, line_number);
                }
                else if (var_name.equal_i("shader"))
                {
                    CString::copy(value.c_str(), out_resource->shader_name, Constants::max_shader_name_length);
                }
            }
            else if (scope == ParserScope::PROPERTIES)
            {
                out_resource->properties.emplace();
                property_i = out_resource->properties.count - 1;
                CString::copy(var_name.c_str(), out_resource->properties[property_i].name, MaterialProperty::max_name_length);
                parse_property(&value, line_number, &out_resource->properties[property_i]);
            }
            else if (scope == ParserScope::TEXTURE_MAP)
            {
                TextureMapResourceData* map = &out_resource->maps[texture_map_i];

                if (var_name.equal_i("name"))
                {
                    CString::copy(value.c_str(), map->name, Constants::max_texture_name_length);
                }
                if (var_name.equal_i("texture_name"))
                {
                    CString::copy(value.c_str(), map->texture_name, Constants::max_texture_name_length);
                }
                else if (var_name.equal_i("filter_min"))
                {
                    map->filter_min = parse_texture_filter(&value, line_number);
                }
                else if (var_name.equal_i("filter_mag"))
                {
                    map->filter_mag = parse_texture_filter(&value, line_number);
                }
                else if (var_name.equal_i("repeat_u"))
                {
                    map->repeat_u = parse_texture_repeat(&value, line_number);
                }
                else if (var_name.equal_i("repeat_v"))
                {
                    map->repeat_v = parse_texture_repeat(&value, line_number);
                }
                else if (var_name.equal_i("repeat_w"))
                {
                    map->repeat_w = parse_texture_repeat(&value, line_number);
                }
            }

            line_number++;
        }

        FileSystem::file_close(&f);

        if (!success)
            material_loader_unload(out_resource);            

		return true;

	}

    bool32 material_loader_import_obj_material_library_file(const char* file_path)
    {

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(file_path, FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("import_obj_material_library_file - Failed to open file for loading material '%s'", file_path);
            return false;
        }

        MaterialResourceData current_resource = {};
        current_resource.auto_release = true;
        current_resource.type = MaterialType::PHONG;
        CString::copy(Renderer::RendererConfig::builtin_shader_name_material_phong, current_resource.shader_name, Constants::max_shader_name_length);
        current_resource.maps.init(3, 0);
        current_resource.properties.init(2, 0);

        uint32 file_size = FileSystem::get_file_size32(&f);
        String file_content(file_size + 1);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(&f, file_content, &bytes_read))
        {
            SHMERRORV("Failed to read from file: '%s'.", file_path);
            return false;
        }

        // Read each line of the file.
        bool32 hit_name = false;

        String line(512);
        uint32 line_number = 1;

        String identifier;
        String values;

        bool32 diffuse_parsed = false;
        bool32 specular_parsed = false;
        bool32 normal_parsed = false;

        TextureMapResourceData default_map = {};
        default_map.filter_min = default_map.filter_mag = TextureFilter::LINEAR;
        default_map.repeat_u = default_map.repeat_v = default_map.repeat_w = TextureRepeat::REPEAT;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content.c_str(), line, &continue_ptr))
        {

            line.trim();
            if (line.len() < 1 || line[0] == '#')
                continue;

            int32 first_space_i = line.index_of(' ');
            if (first_space_i == -1)
                continue;

            mid(identifier, line.c_str(), 0, first_space_i);
            mid(values, line.c_str(), first_space_i + 1);
            identifier.trim();
            values.trim();

            if (identifier == "Ka")
            {

            }
            else if (identifier == "Kd")
            {
                MaterialProperty* prop = &current_resource.properties[current_resource.properties.emplace()];
                CString::copy("diffuse_color", prop->name, prop->max_name_length);
                prop->type = MaterialPropertyType::FLOAT32_4;
                CString::parse_arr(values.c_str(), ' ', 3, prop->f32);
                prop->f32[3] = 1.0f;
            }
            else if (identifier == "Ks")
            {
            }
            else if (identifier == "Ns")
            {
                MaterialProperty* prop = &current_resource.properties[current_resource.properties.emplace()];
                CString::copy("shininess", prop->name, prop->max_name_length);
                prop->type = MaterialPropertyType::FLOAT32;
                CString::parse(values.c_str(), &prop->f32[0]);
                if (prop->f32[0] <= 0.0f)
                    prop->f32[0] = 8.0f;
            }
            else if (identifier.nequal_i("map_Kd", 6) && !diffuse_parsed)
            {
                values.left_of_last('.');
                values.right_of_last('/');
                values.right_of_last('\\');
                TextureMapResourceData* map = &current_resource.maps[current_resource.maps.emplace()];
                *map = default_map;
                CString::copy(values.c_str(), map->texture_name, Constants::max_texture_name_length);
                CString::copy("diffuse", map->name, Constants::max_texture_name_length);
                diffuse_parsed = true;
            }
            else if (identifier.nequal_i("map_Ks", 6) && !specular_parsed)
            {
                values.left_of_last('.');
                values.right_of_last('/');
                values.right_of_last('\\');
                TextureMapResourceData* map = &current_resource.maps[current_resource.maps.emplace()];
                *map = default_map;
                CString::copy(values.c_str(), map->texture_name, Constants::max_texture_name_length);
                CString::copy("specular", map->name, Constants::max_texture_name_length);
                diffuse_parsed = true;
            }
            else if ((identifier.nequal_i("map_bump", 8) || identifier.nequal_i("bump", 4)) && !normal_parsed)
            {
                values.left_of_last('.');
                values.right_of_last('/');
                values.right_of_last('\\');
                TextureMapResourceData* map = &current_resource.maps[current_resource.maps.emplace()];
                *map = default_map;
                CString::copy(values.c_str(), map->texture_name, Constants::max_texture_name_length);
                CString::copy("normal", map->name, Constants::max_texture_name_length);
                normal_parsed = true;
            }
            else if (identifier.nequal_i("newmtl", 6))
            {               

                if (hit_name)
                {
                    if (!write_shmt_file(&current_resource)) {
                        SHMERROR("Unable to write shmt file.");
                        return false;
                    }

                    current_resource.maps.clear();
                    current_resource.properties.clear();
                    current_resource.name[0] = 0;

                    diffuse_parsed = false;
                    specular_parsed = false;
                    normal_parsed = false;
                }

                hit_name = true;
                CString::copy(values.c_str(), current_resource.name, Constants::max_material_name_length);
            }

            line_number++;
        }

        if (hit_name)
        {
            if (!write_shmt_file(&current_resource)) {
                SHMERROR("Unable to write shmt file.");
                return false;
            }
        }

        current_resource.maps.free_data();
        current_resource.properties.free_data();

        FileSystem::file_close(&f);
        return true;

    }

    // TODO: Move this function to a proper place and look for dynamic directory
    static bool32 write_shmt_file(MaterialResourceData* resource)
    {

        const char* format = "%s%s%s%s";
        char full_filepath[Constants::max_filepath_length];

        CString::safe_print_s<const char*, const char*, const char*, const char*>
            (full_filepath, Constants::max_filepath_length, format, Engine::get_assets_base_path(), loader_type_path, resource->name, ".shmt");

        FileSystem::FileHandle f;

        if (!FileSystem::file_open(full_filepath, FILE_MODE_WRITE, &f)) {
            SHMERRORV("Error opening material file for writing: '%s'", full_filepath);
            return false;
        }
        SHMDEBUGV("Writing .shmt file '%s'...", full_filepath);

        char line_buffer[512];
        String content(1024);
        content += "#material file\n";
        content += "\n";
        content += "version=0.1\n";
        content += "name=";
        content += resource->name;
        content += "\n";

        content += "type=";
        if (resource->type == MaterialType::PHONG)
            content += "phong\n";
        else if (resource->type == MaterialType::UI)
            content += "ui\n";
        else if (resource->type == MaterialType::PBR)
            content += "pbr\n";
        else if (resource->type == MaterialType::CUSTOM)
            content += "custom\n";
        else
            content += "phong\n";

        content += "shader=";
        content += resource->shader_name;
        content += "\n\n";

        content += "[Properties]\n";
        for (uint32 i = 0; i < resource->properties.count; i++)
        {
            switch (resource->properties[i].type)
            {
            case MaterialPropertyType::UINT8:
            {
                CString::print_s(line_buffer, 512, "%s=u8/%hhu\n", resource->properties[i].name, resource->properties[i].u8[0]);                
                break;
            }
            case MaterialPropertyType::UINT16:
            {
                CString::print_s(line_buffer, 512, "%s=u16/%hu\n", resource->properties[i].name, resource->properties[i].u16[0]);
                break;
            }
            case MaterialPropertyType::UINT32:
            {
                CString::print_s(line_buffer, 512, "%s=u32/%u\n", resource->properties[i].name, resource->properties[i].u32[0]);
                break;
            }
            case MaterialPropertyType::UINT64:
            {
                CString::print_s(line_buffer, 512, "%s=u64/%lu\n", resource->properties[i].name, resource->properties[i].u64[0]);
                break;
            }
            case MaterialPropertyType::INT8:
            {
                CString::print_s(line_buffer, 512, "%s=i8/%hhu\n", resource->properties[i].name, resource->properties[i].i8[0]);
                break;
            }
            case MaterialPropertyType::INT16:
            {
                CString::print_s(line_buffer, 512, "%s=i16/%hu\n", resource->properties[i].name, resource->properties[i].i16[0]);
                break;
            }
            case MaterialPropertyType::INT32:
            {
                CString::print_s(line_buffer, 512, "%s=i32/%u\n", resource->properties[i].name, resource->properties[i].i32[0]);
                break;
            }
            case MaterialPropertyType::INT64:
            {
                CString::print_s(line_buffer, 512, "%s=i64/%lu\n", resource->properties[i].name, resource->properties[i].i64[0]);
                break;
            }
            case MaterialPropertyType::FLOAT32:
            {
                CString::print_s(line_buffer, 512, "%s=f32/%f6\n", resource->properties[i].name, resource->properties[i].f32[0]);
                break;
            }
            case MaterialPropertyType::FLOAT64:
            {
                CString::print_s(line_buffer, 512, "%s=f64/%lf6\n", resource->properties[i].name, resource->properties[i].f64[0]);
                break;
            }
            case MaterialPropertyType::FLOAT32_2:
            {
                CString::print_s(line_buffer, 512, "%s=vec2/%f6 %f6\n", resource->properties[i].name, 
                    resource->properties[i].f32[0],
                    resource->properties[i].f32[1]);
                break;
            }
            case MaterialPropertyType::FLOAT32_3:
            {
                CString::print_s(line_buffer, 512, "%s=vec3/%f6 %f6 %f6\n", resource->properties[i].name, 
                    resource->properties[i].f32[0],
                    resource->properties[i].f32[1],
                    resource->properties[i].f32[2]);
                break;
            }
            case MaterialPropertyType::FLOAT32_4:
            {
                CString::print_s(line_buffer, 512, "%s=vec4/%f6 %f6 %f6 %f6\n", resource->properties[i].name, 
                    resource->properties[i].f32[0],
                    resource->properties[i].f32[1],
                    resource->properties[i].f32[2],
                    resource->properties[i].f32[3]);
                break;
            }
            case MaterialPropertyType::FLOAT32_16:
            {
                CString::print_s(line_buffer, 512, "%s=mat4/%f6 %f6 %f6 %f6 %f6 %f6 %f6 %f6 %f6 %f6 %f6 %f6 %f6 %f6 %f6 %f6\n", resource->properties[i].name, 
                    resource->properties[i].f32[0],
                    resource->properties[i].f32[1],
                    resource->properties[i].f32[2],
                    resource->properties[i].f32[3],
                    resource->properties[i].f32[4],
                    resource->properties[i].f32[5],
                    resource->properties[i].f32[6],
                    resource->properties[i].f32[7],
                    resource->properties[i].f32[8],
                    resource->properties[i].f32[9],
                    resource->properties[i].f32[10],
                    resource->properties[i].f32[11],
                    resource->properties[i].f32[12],
                    resource->properties[i].f32[13],
                    resource->properties[i].f32[14],
                    resource->properties[i].f32[15]);
                break;
            }
            default:
            {
                continue;
            }
            }
            content += line_buffer;
        }
        content += "[/]\n\n";

        for (uint32 i = 0; i < resource->maps.count; i++)
        {
            content += "[TextureMap]\n";

            content += "name=";
            content += resource->maps[i].name;
            content += "\n";

            content += "texture_name=";
            content += resource->maps[i].texture_name;
            content += "\n";

            content += "filter_min=";
            content += texture_filter_names[resource->maps[i].filter_min];
            content += "\n";

            content += "filter_mag=";
            content += texture_filter_names[resource->maps[i].filter_mag];
            content += "\n";

            content += "repeat_u=";
            content += texture_repeat_names[resource->maps[i].repeat_u];
            content += "\n";

            content += "repeat_v=";
            content += texture_repeat_names[resource->maps[i].repeat_v];
            content += "\n";

            content += "repeat_w=";
            content += texture_repeat_names[resource->maps[i].repeat_w];
            content += "\n";

            content += "[/]\n\n";
        }

        uint32 bytes_written = 0;
        content.update_len();
        FileSystem::write(&f, content.len(), &content[0], &bytes_written);

        FileSystem::file_close(&f);

        return true;

    }

	void material_loader_unload(MaterialResourceData* resource)
	{
        resource->properties.free_data();
        resource->maps.free_data();
	}

}