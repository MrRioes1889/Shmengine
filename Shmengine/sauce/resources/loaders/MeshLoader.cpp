#include "ShaderLoader.hpp"

#include "resources/ResourceTypes.hpp"
#include "LoaderUtils.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "renderer/RendererGeometry.hpp"
#include "platform/FileSystem.hpp"

enum class MeshFileType {
    NOT_FOUND,
    KSM,
    OBJ
};

struct SupportedMeshFileType {
    const char* extension;
    MeshFileType type;
    bool32 is_binary;
};

struct MeshVertexIndexData {
    uint32 position_index;
    uint32 normal_index;
    uint32 texcoord_index;
};

struct MeshFaceData {
    MeshVertexIndexData vertices[3];
};

struct MeshGroupData {
    Darray<MeshFaceData> faces;
};

namespace ResourceSystem
{

    static bool32 import_obj_file(FileSystem::FileHandle* obj_file, const char* obj_filepath, const char* out_ksm_filename, Darray<GeometrySystem::GeometryConfig>& out_geometries_darray);
    static void process_subobject(const Darray<Math::Vec3f>& positions, const Darray<Math::Vec3f>& normals, const Darray<Math::Vec2f>& tex_coords, const Darray<MeshFaceData>& faces, GeometrySystem::GeometryConfig* out_data);
    static bool32 import_obj_material_library_file(const char* mtl_file_path);
    static bool32 write_shmt_file(const char* directory, MaterialConfig* config);

    static bool32 load_ksm_file(FileSystem::FileHandle* ksm_file, Darray<GeometrySystem::GeometryConfig>& out_geometries_darray);
    static bool32 write_ksm_file(const char* path, const char* name, Darray<GeometrySystem::GeometryConfig>& geometries);
    

    bool32 mesh_loader_load(ResourceLoader* loader, const char* name, Resource* out_resource)
    {

        const char* format = "%s%s%s";
        String full_filepath_wo_extension(MAX_FILEPATH_LENGTH);

        safe_print_s<const char*, const char*, const char*>
            (full_filepath_wo_extension, format, get_base_path(), loader->type_path, name);

        const uint32 supported_file_type_count = 2;
        SupportedMeshFileType supported_file_types[supported_file_type_count] = {};
        supported_file_types[0] = { ".ksm", MeshFileType::KSM, true };
        supported_file_types[1] = { ".obj", MeshFileType::OBJ, false };

        String full_filepath(MAX_FILEPATH_LENGTH);
        MeshFileType file_type = MeshFileType::NOT_FOUND;
        for (uint32 i = 0; i < supported_file_type_count; i++)
        {
            full_filepath = full_filepath_wo_extension;
            full_filepath.append(supported_file_types[i].extension);
            if (FileSystem::file_exists(full_filepath.c_str()))
            {
                file_type = supported_file_types[i].type;
                break;
            }
        }

        if (file_type == MeshFileType::NOT_FOUND) {
            SHMERRORV("Mesh resource loader failed to find file '%s' with any valid extensions.", full_filepath_wo_extension.c_str());
            return false;
        }

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath.c_str(), FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("mesh_loader_load - Failed to open file for loading mesh '%s'", full_filepath.c_str());
            return false;
        }

        Darray<GeometrySystem::GeometryConfig> resource_data;

        bool32 res = false;
        switch (file_type)
        {
        case MeshFileType::OBJ:
        {
            String ksm_filepath(MAX_FILEPATH_LENGTH);
            ksm_filepath = full_filepath_wo_extension;
            ksm_filepath.append(".ksm");
            res = import_obj_file(&f, full_filepath.c_str(), ksm_filepath.c_str(), resource_data);
            break;
        }
        case MeshFileType::KSM:
        {
            res = load_ksm_file(&f, resource_data);
            break;
        }
        }

        FileSystem::file_close(&f);

        if (!res)
        {
            SHMERRORV("Failed to process mesh file '%s'!", full_filepath.c_str());
            out_resource->data = 0;
            out_resource->data_size = 0;
            return res;
        }

        out_resource->allocation_tag = (AllocationTag)resource_data.allocation_tag;
        out_resource->data_size = resource_data.count;
        out_resource->data = resource_data.transfer_data();      

        return true;

    }

    void mesh_loader_unload(ResourceLoader* loader, Resource* resource)
    {
        if (resource->data)
        {
            GeometrySystem::GeometryConfig* data = (GeometrySystem::GeometryConfig*)resource->data;
            (*data).~GeometryConfig();
        }

        resource_unload(loader, resource);
    }

    static bool32 import_obj_file(FileSystem::FileHandle* obj_file, const char* obj_filepath, const char* out_ksm_filename, Darray<GeometrySystem::GeometryConfig>& out_geometries_darray)
    {
        
        uint32 file_size = FileSystem::get_file_size32(obj_file);
        String file_content(file_size + 1);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(obj_file, file_content, &bytes_read))
        {
            SHMERRORV("import_obj_file - failed to read from file: '%s'.", obj_filepath);
            return false;
        }

        Darray<Math::Vec3f> positions(0x4000, 0, AllocationTag::MAIN);
        Darray<Math::Vec3f> normals(0x4000, 0, AllocationTag::MAIN);
        Darray<Math::Vec2f> tex_coords(0x4000, 0, AllocationTag::MAIN);

        Darray<MeshGroupData> groups(4, 0, AllocationTag::MAIN);

        String material_file_name(MAX_FILEPATH_LENGTH);

        String name(MAX_FILEPATH_LENGTH);
        uint32 current_mat_name_count = 0;
        Darray<String> material_names(64, 0, AllocationTag::MAIN);

        // Read each line of the file.
        String line(512);
        uint64 line_length = 0;
        uint32 line_number = 1;
        String identifier;
        String values;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content, line, &continue_ptr))
        {

            if (line.len() < 1 || line[0] == '#') {
                continue;
            }

            int32 first_space_i = line.index_of(' ');
            if (first_space_i == -1)
                continue;
            
            identifier = mid(line, 0, first_space_i);
            values = mid(line, first_space_i + 1);
            identifier.trim();
            values.trim();

            if (identifier.len() == 0)
                continue;      

            if (identifier == "v")
            {
                Math::Vec3f pos;
                CString::parse(values.c_str(), pos);
                positions.push(pos);
            }
            else if (identifier == "vn")
            {
                Math::Vec3f norm;
                CString::parse(values.c_str(), norm);
                normals.push(norm);
            }
            else if (identifier == "vt")
            {
                Math::Vec2f tex_c;
                CString::parse(values.c_str(), tex_c);
                tex_coords.push(tex_c);
            }
            else if (identifier == "s")
            {
            }
            else if (identifier == "f")
            {
                MeshFaceData face;

                if (normals.count == 0 || tex_coords.count == 0)
                {
                    CString::scan(values.c_str(), "%u %u %u",
                        &face.vertices[0].position_index,
                        &face.vertices[1].position_index,
                        &face.vertices[2].position_index);
                }
                else
                {
                    CString::scan(values.c_str(), "%u/%u/%u %u/%u/%u %u/%u/%u",
                        &face.vertices[0].position_index,
                        &face.vertices[0].texcoord_index,
                        &face.vertices[0].normal_index,
                        &face.vertices[1].position_index,
                        &face.vertices[1].texcoord_index,
                        &face.vertices[1].normal_index,
                        &face.vertices[2].position_index,
                        &face.vertices[2].texcoord_index,
                        &face.vertices[2].normal_index);
                }

                uint32 group_index = groups.count - 1;
                groups[group_index].faces.push(face);
            }
            else if (identifier == "mtllib")
            {
                material_file_name = values;
            }
            else if (identifier == "g")
            {
                for (uint32 i = 0; i < groups.count; i++)
                {
                    GeometrySystem::GeometryConfig* new_data = out_geometries_darray.push({});
                    CString::copy(Geometry::max_name_length, new_data->name, name.c_str());
                    if (i > 0)
                        CString::append(Geometry::max_name_length, new_data->name, CString::to_string(i));
                    CString::copy(Material::max_name_length, new_data->material_name, material_names[i].c_str());

                    process_subobject(positions, normals, tex_coords, groups[i].faces, new_data);

                    groups[i].faces.free_data();
                }

                material_names.clear();
                groups.clear();
                name = values;
            }
            else if (identifier == "usemtl")
            {
                MeshGroupData* new_group = groups.push({});
                new_group->faces.init(0xF000, 0, AllocationTag::MAIN);      

                material_names.push(values);
            }

            line_number++;
        }

        for (uint32 i = 0; i < groups.count; i++)
        {
            
            GeometrySystem::GeometryConfig* new_data = out_geometries_darray.push({});
            CString::copy(Geometry::max_name_length, new_data->name, name.c_str());
            if (i > 0)
                CString::append(Geometry::max_name_length, new_data->name, CString::to_string(i));
            CString::copy(Material::max_name_length, new_data->material_name, material_names[i].c_str());

            process_subobject(positions, normals, tex_coords, groups[i].faces, new_data);

            groups[i].faces.free_data();
        }

        groups.free_data();
        positions.free_data();
        normals.free_data();
        tex_coords.free_data();

        if (material_file_name.len() > 0)
        {
            String directory = mid(out_ksm_filename, 0, CString::index_of_last(out_ksm_filename, '/') + 1);
            directory.append(material_file_name);
            if (!import_obj_material_library_file(material_file_name.c_str()))
                SHMERRORV("Error reading obj mtl file '%s'.", material_file_name.c_str());
        }

        // De-duplicate geometry

        for (uint32 i = 0; i < out_geometries_darray.count; ++i) {
            GeometrySystem::GeometryConfig* g = out_geometries_darray.data;
            SHMDEBUGV("Geometry de-duplication process starting on geometry object named '%s'...", g->name);

            Renderer::geometry_deduplicate_vertices(*g);

            // TODO: Maybe shrink down vertex array to count after deduplication!
        }

        // Output a ksm file, which will be loaded in the future.
        return write_ksm_file(out_ksm_filename, name.c_str(), out_geometries_darray);

    }

    static void process_subobject(const Darray<Math::Vec3f>& positions, const Darray<Math::Vec3f>& normals, const Darray<Math::Vec2f>& tex_coords, const Darray<MeshFaceData>& faces, GeometrySystem::GeometryConfig* out_data)
    {

        out_data->vertex_count = 0;
        out_data->vertex_size = sizeof(Renderer::Vertex3D);
        bool32 extent_set = false;
        out_data->min_extents = {};
        out_data->max_extents = {};

        Darray<Renderer::Vertex3D> vertices(64, 0, AllocationTag::MAIN);
        Darray<uint32> indices(64, 0, AllocationTag::MAIN);

        bool32 skip_normals = normals.count == 0;
        bool32 skip_tex_coords = tex_coords.count == 0;

        if (skip_normals)
            SHMWARN("No normals found for mesh!");

        if (skip_tex_coords)
            SHMWARN("No texture coordinates found for mesh!");

        for (uint32 f = 0; f < faces.count; f++)
        {

            const MeshFaceData& face = faces[f];

            for (uint32 v = 0; v < 3; v++)
            {
                const MeshVertexIndexData& index_data = face.vertices[v];
                indices.push(v + (f * 3));

                Renderer::Vertex3D vert = {};

                Math::Vec3f pos = positions[index_data.position_index - 1];
                vert.position = pos;

                // Check extents - min
                if (pos.x < out_data->min_extents.x || !extent_set) {
                    out_data->min_extents.x = pos.x;
                }
                if (pos.y < out_data->min_extents.y || !extent_set) {
                    out_data->min_extents.y = pos.y;
                }
                if (pos.z < out_data->min_extents.z || !extent_set) {
                    out_data->min_extents.z = pos.z;
                }

                // Check extents - max
                if (pos.x > out_data->max_extents.x || !extent_set) {
                    out_data->max_extents.x = pos.x;
                }
                if (pos.y > out_data->max_extents.y || !extent_set) {
                    out_data->max_extents.y = pos.y;
                }
                if (pos.z > out_data->max_extents.z || !extent_set) {
                    out_data->max_extents.z = pos.z;
                }

                extent_set = true;

                if (skip_normals) {
                    vert.normal = VEC3_ZERO;
                }
                else {
                    vert.normal = normals[index_data.normal_index - 1];
                }

                if (skip_tex_coords) {
                    vert.texcoord = VEC2_ZERO;
                }
                else {
                    vert.texcoord = tex_coords[index_data.texcoord_index - 1];
                }

                // TODO: Color. Hardcode to white for now.
                vert.color = VEC4F_ONE;

                vertices.push(vert);
            }

        }

        for (uint32 i = 0; i < 3; i++)
        {
            out_data->center.e[i] = (out_data->min_extents.e[i] + out_data->max_extents.e[i]) / 2.0f;
        }

        uint32 vertex_count = vertices.count;
        uint32 index_count = indices.count;

        out_data->vertex_count = vertex_count;
        out_data->vertices.init(vertex_count, 0, (AllocationTag)vertices.allocation_tag, vertices.transfer_data());
        out_data->indices.init(index_count, 0, (AllocationTag)indices.allocation_tag, indices.transfer_data());

        Renderer::geometry_generate_tangents(*out_data);

    }

    static bool32 import_obj_material_library_file(const char* mtl_file_path)
    {

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(mtl_file_path, FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("import_obj_material_library_file - Failed to open file for loading material '%s'", mtl_file_path);
            return false;
        }

        MaterialConfig current_config = {};

        uint32 file_size = FileSystem::get_file_size32(&f);
        String file_content(file_size + 1);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(&f, file_content, &bytes_read))
        {
            SHMERRORV("material_loader_load - failed to read from file: '%s'.", mtl_file_path);
            return false;
        }

        // Read each line of the file.
        bool32 hit_name = false;

        String line(512);
        uint32 line_number = 1;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content, line, &continue_ptr))
        {

            line.trim();
            if (line.len() < 1 || line[0] == '#')
                continue;

            int32 first_space_i = line.index_of(' ');
            if (first_space_i == -1)
                continue;

            String identifier = mid(line, 0, first_space_i);
            String values = mid(line, first_space_i + 1);
            identifier.trim();
            values.trim();

            if (identifier == "Ka")
            {

            }
            else if (identifier == "Kd")
            {
                CString::scan(values.c_str(), "%f %f %f", &current_config.diffuse_color.r, &current_config.diffuse_color.g, &current_config.diffuse_color.b);
                current_config.diffuse_color.a = 1.0f;
            }
            else if (identifier == "Ks")
            {
                float32 spec;
                CString::scan(values.c_str(), "%f %f %f", &spec, &spec, &spec);
            }
            else if (identifier == "Ns")
            {
                CString::parse_f32(values.c_str(), current_config.shininess);
            }
            else if (identifier.nequal_i("map_Kd", 6))
            {
                values.left_of_last('.');
                values.right_of_last('/');
                values.right_of_last('\\');
                CString::copy(Texture::max_name_length, current_config.diffuse_map_name, values.c_str());
            }
            else if (identifier.nequal_i("map_Ks", 6))
            {
                values.left_of_last('.');
                values.right_of_last('/');
                values.right_of_last('\\');
                CString::copy(Texture::max_name_length, current_config.specular_map_name, values.c_str());
            }
            else if (identifier.nequal_i("map_bump", 6) || identifier.nequal_i("bump", 6))
            {
                values.left_of_last('.');
                values.right_of_last('/');
                values.right_of_last('\\');
                CString::copy(Texture::max_name_length, current_config.normal_map_name, values.c_str());
            }
            else if (identifier.nequal_i("newmtl", 6))
            {                

                current_config.shader_name = Renderer::RendererConfig::builtin_shader_name_material;

                if (current_config.shininess == 0.0f)
                    current_config.shininess = 8.0f;

                if (hit_name)
                {
                    String directory = left_of_last(mtl_file_path, '/');
                    directory.left_of_last('\\');
                    if (!write_shmt_file(directory.c_str(), &current_config)) {
                        SHMERROR("Unable to write kmt file.");
                        return false;
                    }

                    current_config = {};
                }

                hit_name = true;
                CString::copy(Material::max_name_length, current_config.name, values.c_str());
            }

            line_number++;
        }

        if (hit_name)
        {
            current_config.shader_name = Renderer::RendererConfig::builtin_shader_name_material;

            if (current_config.shininess == 0.0f)
                current_config.shininess = 8.0f;

            if (!write_shmt_file(mtl_file_path, &current_config)) {
                SHMERROR("Unable to write kmt file.");
                return false;
            }
        }
        
        FileSystem::file_close(&f);
        return true;

    }

    // TODO: Move this function to a proper place and look for dynamic directory
    static bool32 write_shmt_file(const char* directory, MaterialConfig* config)
    {
        
        const char* format_str = "%s../materials/%s%s";
        FileSystem::FileHandle f;

        String full_file_path = directory;
        full_file_path.append("/../materials/");
        full_file_path.append(config->name);
        full_file_path.append(".shmt");

        if (!FileSystem::file_open(full_file_path.c_str(), FILE_MODE_WRITE, &f)) {
            SHMERRORV("Error opening material file for writing: '%s'", full_file_path.c_str());
            return false;
        }
        SHMDEBUGV("Writing .shmt file '%s'...", full_file_path.c_str());

        char line_buffer[512];
        String content(512);
        content += "#material file\n";
        content += "\n";
        content += "version=0.1\n";
        content += "name=";
        content += config->name;
        content += "\n";
  
        CString::print_s(line_buffer, 512, "diffuse_colour=%.6f %.6f %.6f %.6f\n", config->diffuse_color.r, config->diffuse_color.g, config->diffuse_color.b, config->diffuse_color.a);
        content += line_buffer;

        CString::print_s(line_buffer, 512, "shininess=%f\n", config->shininess);
        content += line_buffer;

        if (config->diffuse_map_name[0]) {
            CString::print_s(line_buffer, 512, "diffuse_map_name=%s\n", config->diffuse_map_name);
            content += line_buffer;
        }
        if (config->specular_map_name[0]) {
            CString::print_s(line_buffer, 512, "specular_map_name=%s\n", config->specular_map_name);
            content += line_buffer;
        }
        if (config->normal_map_name[0]) {
            CString::print_s(line_buffer, 512, "normal_map_name=%s\n", config->normal_map_name);
            content += line_buffer;
        }

        CString::print_s(line_buffer, 512, "shader=%s\n", config->shader_name.c_str());
        content += line_buffer;

        uint32 bytes_written = 0;
        FileSystem::write(&f, content.len(), &content[0], &bytes_written);

        FileSystem::file_close(&f);

        return true;

    }

    static bool32 load_ksm_file(FileSystem::FileHandle* ksm_file, Darray<GeometrySystem::GeometryConfig>& out_geometries_darray)
    {
        return true;
    }

    static bool32 write_ksm_file(const char* path, const char* name, Darray<GeometrySystem::GeometryConfig>& geometries)
    {
        return true;
    }

	ResourceLoader mesh_resource_loader_create()
	{
        ResourceLoader loader;
        loader.type = ResourceType::MESH;
        loader.custom_type = 0;
        loader.load = mesh_loader_load;
        loader.unload = mesh_loader_unload;
        loader.type_path = "models/";

        return loader;
	}
}