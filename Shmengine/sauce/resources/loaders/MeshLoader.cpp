#include "MeshLoader.hpp"

#include "core/Engine.hpp"
#include "MaterialLoader.hpp"
#include "systems/MaterialSystem.hpp"
#include "resources/Mesh.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "renderer/Geometry.hpp"
#include "platform/FileSystem.hpp"

enum class MeshFileType {
    NOT_FOUND,
    SHMESH,
    OBJ
};

struct SupportedMeshFileType {
    const char* extension;
    MeshFileType type;
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


struct ShmeshFileHeader
{
    uint16 version;
    uint16 name_length;
    uint32 name_offset;
    uint32 geometry_count;
    uint32 geometries_offset;
};

struct ShmeshFileGeometryHeader
{    
    uint16 name_length;
    uint16 material_name_length;
    uint32 name_offset;   
    uint32 material_name_offset;   
    Math::Vec3f center;
    Math::Vec3f min_extents;
    Math::Vec3f max_extents;
    uint32 vertices_indices_offset;
    uint32 vertex_size;
    uint32 vertex_count;
    uint32 index_size;
    uint32 index_count;
};

namespace ResourceSystem
{

    static const char* loader_type_path = "models/";

    static bool8 import_obj_file(FileSystem::FileHandle* obj_file, const char* obj_filepath, const char* mesh_name, const char* out_shmesh_filename, MeshResourceData* out_resource);
    static void process_subobject(const Darray<Math::Vec3f>& positions, const Darray<Math::Vec3f>& normals, const Darray<Math::Vec2f>& tex_coords, const Darray<MeshFaceData>& faces, GeometryResourceData* out_data);

    static bool8 load_shmesh_file(FileSystem::FileHandle* shmesh_file, const char* shmesh_filepath, MeshResourceData* out_resource);
    static bool8 write_shmesh_file(const char* path, const char* name, MeshResourceData* resource);

    static void geometry_resource_deduplicate_vertices(GeometryResourceData* geo);

    bool8 mesh_loader_load(const char* name, MeshResourceData* out_resource)
    {

        const char* format = "%s%s%s";
        String full_filepath_wo_extension(Constants::max_filepath_length);

        full_filepath_wo_extension.safe_print_s<const char*, const char*, const char*>
            (format, Engine::get_assets_base_path(), loader_type_path, name);

        const uint32 supported_file_type_count = 2;
        SupportedMeshFileType supported_file_types[supported_file_type_count] = {};
        supported_file_types[0] = { ".shmesh", MeshFileType::SHMESH };
        supported_file_types[1] = { ".obj", MeshFileType::OBJ };

        String full_filepath(Constants::max_filepath_length);
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

        out_resource->geometries.init(1, 0, AllocationTag::Resource);

        bool8 res = false;
        switch (file_type)
        {
        case MeshFileType::OBJ:
        {
            String shmesh_filepath(Constants::max_filepath_length);
            shmesh_filepath = full_filepath_wo_extension;
            shmesh_filepath.append(".shmesh");
            res = import_obj_file(&f, full_filepath.c_str(), name, shmesh_filepath.c_str(), out_resource);
            
            break;
        }
        case MeshFileType::SHMESH:
        {
            res = load_shmesh_file(&f, full_filepath.c_str(), out_resource);
            break;
        }
        default:
        {
            return false;
        }
        }

        FileSystem::file_close(&f);

        if (!res)
        {
            SHMERRORV("Failed to process mesh file '%s'!", full_filepath.c_str());
            mesh_loader_unload(out_resource);
            return res;
        }    

        return true;

    }

    void mesh_loader_unload(MeshResourceData* resource)
    {
        for (uint32 i = 0; i < resource->geometries.count; i++)
        {
            GeometryResourceData* g_config = &resource->geometries[i].geometry_data;
            g_config->indices.free_data();
            g_config->vertices.free_data();
        }
        resource->geometries.free_data();
        resource->mesh_geometry_configs.free_data();
    }

    MeshConfig mesh_loader_get_config_from_resource(MeshResourceData* resource)
    {
        resource->mesh_geometry_configs.free_data();
        resource->mesh_geometry_configs.init(resource->geometries.count, 0);

        for (uint32 i = 0; i < resource->geometries.count; i++)
        {
            MeshGeometryConfig* mesh_config = &resource->mesh_geometry_configs[i];
            mesh_config->geo_config.vertex_size = resource->geometries[i].geometry_data.vertex_size;
            mesh_config->geo_config.vertex_count = resource->geometries[i].geometry_data.vertex_count;
            mesh_config->geo_config.index_count = resource->geometries[i].geometry_data.index_count;
            mesh_config->geo_config.center = resource->geometries[i].geometry_data.center;
            mesh_config->geo_config.extents = resource->geometries[i].geometry_data.extents;
            mesh_config->geo_config.vertices = resource->geometries[i].geometry_data.vertices.data;
            mesh_config->geo_config.indices = resource->geometries[i].geometry_data.indices.data;
            mesh_config->geo_config.name = resource->geometries[i].geometry_data.name;
            mesh_config->material_name = resource->geometries[i].material_name;
        }

        MeshConfig config =
        {
            .g_configs_count = resource->mesh_geometry_configs.capacity,
            .g_configs = resource->mesh_geometry_configs.data,
        };

        return config;
    }

    static bool8 import_obj_file(FileSystem::FileHandle* obj_file, const char* obj_filepath, const char* mesh_name, const char* out_shmesh_filename, MeshResourceData* out_resource)
    {
        
        uint32 file_size = FileSystem::get_file_size32(obj_file);
        String file_content(file_size + 1);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(obj_file, file_content, &bytes_read))
        {
            SHMERRORV("import_obj_file - failed to read from file: '%s'.", obj_filepath);
            return false;
        }

        Darray<Math::Vec3f> positions(0x4000, 0);
        Darray<Math::Vec3f> normals(0x4000, 0);
        Darray<Math::Vec2f> tex_coords(0x4000, 0);

        Darray<MeshGroupData> groups(4, 0);

        String material_file_name(Constants::max_filepath_length);

        String name(Constants::max_filepath_length);
        uint32 current_mat_name_count = 0;
        Darray<String> material_names(64, 0);

        // Read each line of the file.
        String line(512);
        uint64 line_length = 0;
        uint32 line_number = 1;
        String identifier;
        String values;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content.c_str(), line, &continue_ptr))
        {

            if (line.len() < 1 || line[0] == '#') {
                continue;
            }

            int32 first_space_i = line.index_of(' ');
            if (first_space_i == -1)
                continue;
            
            mid(identifier, line.c_str(), 0, first_space_i);
            mid(values, line.c_str(), first_space_i + 1);
            identifier.trim();
            values.trim();

            if (identifier.len() == 0)
                continue;      

            if (identifier == "v")
            {
                Math::Vec3f pos;
                CString::parse(values.c_str(), &pos);
                positions.emplace(pos);
            }
            else if (identifier == "vn")
            {
                Math::Vec3f norm;
                CString::parse(values.c_str(), &norm);
                normals.emplace(norm);
            }
            else if (identifier == "vt")
            {
                Math::Vec2f tex_c;
                CString::parse(values.c_str(), &tex_c);
                tex_coords.emplace(tex_c);
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
                groups[group_index].faces.emplace(face);
            }
            else if (identifier == "mtllib")
            {
                material_file_name = values;
            }
            else if (identifier == "g")
            {
                for (uint32 i = 0; i < groups.count; i++)
                {
                    MeshGeometryResourceData* new_data = &out_resource->geometries[out_resource->geometries.emplace()];
                    if (!name[0])
                    {
                        name = mesh_name;
                        name += "_geo";
                    }
                    CString::copy(name.c_str(), new_data->geometry_data.name, Constants::max_geometry_name_length);

                    CString::copy(material_names[i].c_str(), new_data->material_name, Constants::max_material_name_length);

                    process_subobject(positions, normals, tex_coords, groups[i].faces, &new_data->geometry_data);

                    groups[i].faces.free_data();
                }

                material_names.clear();
                groups.clear();
                name = values;
            }
            else if (identifier == "usemtl")
            {
                MeshGroupData* new_group = &groups[groups.emplace()];
                new_group->faces.init(0xF000, 0);      

                material_names.emplace(values);
            }

            line_number++;
        }

        for (uint32 i = 0; i < groups.count; i++)
        {
            
            MeshGeometryResourceData* new_data = &out_resource->geometries[out_resource->geometries.emplace()];
            CString::copy(name.c_str(), new_data->geometry_data.name, Constants::max_geometry_name_length);
            if (!name[0])
            {
                name = mesh_name;
                name += "_geo";
            }
            CString::copy(name.c_str(), new_data->geometry_data.name, Constants::max_geometry_name_length);

            if (i > 0)
                CString::append(new_data->geometry_data.name, Constants::max_geometry_name_length, CString::to_string(i));
                
            CString::copy(material_names[i].c_str(), new_data->material_name, Constants::max_material_name_length);

            process_subobject(positions, normals, tex_coords, groups[i].faces, &new_data->geometry_data);

            groups[i].faces.free_data();
        }

        groups.free_data();
        positions.free_data();
        normals.free_data();
        tex_coords.free_data();

        if (material_file_name.len() > 0)
        {
            String directory;
            mid(directory, out_shmesh_filename, 0, CString::index_of_last(out_shmesh_filename, '/') + 1);
            directory.append(material_file_name);
            if (!material_loader_import_obj_material_library_file(directory.c_str()))
                SHMERRORV("Error reading obj mtl file '%s'.", material_file_name.c_str());
        }

        // De-duplicate geometry

        for (uint32 i = 0; i < out_resource->geometries.count; ++i) {
            GeometryResourceData* g = &out_resource->geometries[i].geometry_data;
            SHMDEBUGV("Geometry de-duplication process starting on geometry object named '%s'...", g->name);

            geometry_resource_deduplicate_vertices(g);
            Renderer::generate_mesh_tangents(g->vertex_count, (Renderer::Vertex3D*)g->vertices.data, g->index_count, g->indices.data);

            // TODO: Maybe shrink down vertex array to count after deduplication!
        }

        // Output a shmesh file, which will be loaded in the future.
        return write_shmesh_file(out_shmesh_filename, mesh_name, out_resource);

    }

    static void process_subobject(const Darray<Math::Vec3f>& positions, const Darray<Math::Vec3f>& normals, const Darray<Math::Vec2f>& tex_coords, const Darray<MeshFaceData>& faces, GeometryResourceData* out_data)
    {

        out_data->vertex_count = 0;
        out_data->vertex_size = sizeof(Renderer::Vertex3D);
        bool8 extent_set = false;
        out_data->extents.min = {};
        out_data->extents.max = {};

        Darray<Renderer::Vertex3D> vertices(faces.count * 3, 0);
        Darray<uint32> indices(faces.count * 3, 0);

        bool8 skip_normals = normals.count == 0;
        bool8 skip_tex_coords = tex_coords.count == 0;

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
                if (pos.x < out_data->extents.min.x || !extent_set) {
                    out_data->extents.min.x = pos.x;
                }
                if (pos.y < out_data->extents.min.y || !extent_set) {
                    out_data->extents.min.y = pos.y;
                }
                if (pos.z < out_data->extents.min.z || !extent_set) {
                    out_data->extents.min.z = pos.z;
                }

                // Check extents - max
                if (pos.x > out_data->extents.max.x || !extent_set) {
                    out_data->extents.max.x = pos.x;
                }
                if (pos.y > out_data->extents.max.y || !extent_set) {
                    out_data->extents.max.y = pos.y;
                }
                if (pos.z > out_data->extents.max.z || !extent_set) {
                    out_data->extents.max.z = pos.z;
                }

                extent_set = true;

                if (skip_normals) {
                    vert.normal = VEC3_ZERO;
                }
                else {
                    vert.normal = normals[index_data.normal_index - 1];
                }

                if (skip_tex_coords) {
                    vert.tex_coords = VEC2_ZERO;
                }
                else {
                    vert.tex_coords = tex_coords[index_data.texcoord_index - 1];
                }

                // TODO: Color. Hardcode to white for now.
                vert.color = VEC4F_ONE;

                vertices.emplace(vert);
            }

        }

        for (uint32 i = 0; i < 3; i++)
        {
            out_data->center.e[i] = (out_data->extents.min.e[i] + out_data->extents.max.e[i]) / 2.0f;
        }

        out_data->vertex_count = vertices.count;
        out_data->index_count = indices.count;

        Renderer::Vertex3D* vertices_ptr = vertices.transfer_data();
        uint32* indices_ptr = indices.transfer_data();
        out_data->vertices.init(out_data->vertex_count * sizeof(Renderer::Vertex3D), 0, (AllocationTag)vertices.allocation_tag, vertices_ptr);
        out_data->indices.init(out_data->index_count, 0, (AllocationTag)indices.allocation_tag, indices_ptr);

    }

    static bool8 write_shmesh_file(const char* path, const char* name, MeshResourceData* resource)
    {

        FileSystem::FileHandle f;

        if (!FileSystem::file_open(path, FILE_MODE_WRITE, &f)) {
            SHMERRORV("Error opening material file for writing: '%s'", path);
            return false;
        }
        SHMDEBUGV("Writing .shmesh file '%s'...", path);

        uint64 written_total = 0;
        uint32 written = 0;

        ShmeshFileHeader file_header = {};
        file_header.version = 1;
        file_header.name_length = (uint16)CString::length(name);
        file_header.geometry_count = resource->geometries.count;

        file_header.name_offset = sizeof(ShmeshFileHeader);
        file_header.geometries_offset = file_header.name_offset + file_header.name_length;

        FileSystem::write(&f, sizeof(file_header), &file_header, &written);
        written_total += written;

        FileSystem::write(&f, file_header.name_length, name, &written);
        written_total += written;


        for (uint32 i = 0; i < resource->geometries.count; i++)
        {
            MeshGeometryResourceData& config = resource->geometries[i];
            GeometryResourceData& g_data = config.geometry_data;

            ShmeshFileGeometryHeader geo_header = {};
            geo_header.center = g_data.center;
            geo_header.min_extents = g_data.extents.min;
            geo_header.max_extents = g_data.extents.max;
            geo_header.vertex_size = g_data.vertex_size;
            geo_header.vertex_count = g_data.vertex_count;
            geo_header.index_size = sizeof(uint32);
            geo_header.index_count = g_data.index_count;
            geo_header.name_length = (uint16)CString::length(g_data.name);
            geo_header.material_name_length = (uint16)CString::length(config.material_name);

            geo_header.name_offset = sizeof(geo_header);
            geo_header.material_name_offset = geo_header.name_offset + geo_header.name_length;
            geo_header.vertices_indices_offset = geo_header.material_name_offset + geo_header.material_name_length;

            FileSystem::write(&f, sizeof(geo_header), &geo_header, &written);
            written_total += written;

            FileSystem::write(&f, geo_header.name_length, g_data.name, &written);
            written_total += written;

            FileSystem::write(&f, geo_header.material_name_length, config.material_name, &written);
            written_total += written;

            FileSystem::write(&f, geo_header.vertex_size * geo_header.vertex_count, g_data.vertices.data, &written);
            written_total += written;

            FileSystem::write(&f, geo_header.index_size * geo_header.index_count, g_data.indices.data, &written);
            written_total += written;
        }

        FileSystem::file_close(&f);

        return true;

    }



    static bool8 load_shmesh_file(FileSystem::FileHandle* shmesh_file, const char* shmesh_filepath, MeshResourceData* out_resource)
    {

        uint32 file_size = FileSystem::get_file_size32(shmesh_file);
        Buffer file_content(file_size, 0);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(shmesh_file, file_content.data, (uint32)file_content.size, &bytes_read))
        {
            SHMERRORV("load_shmesh_file - failed to read from file: '%s'.", shmesh_filepath);
            return false;
        }

        SHMTRACEV("Importing shmesh file: '%s'.", shmesh_filepath);

        uint8* read_ptr = (uint8*)file_content.data;
        uint64 read_bytes = 0;

        auto check_buffer_size = [&read_bytes, &file_content](uint64 requested_size)
            {
                SHMASSERT_MSG(read_bytes + requested_size <= file_content.size, "Tried to read outside of buffers memory! File formatting might be corrupted.")
            };

        check_buffer_size(sizeof(ShmeshFileHeader));
        ShmeshFileHeader* file_header = (ShmeshFileHeader*)&read_ptr[read_bytes];
        read_bytes += sizeof(ShmeshFileHeader);

        check_buffer_size(file_header->name_length);
        String name;
        name.copy_n((char*)&read_ptr[read_bytes], (uint32)file_header->name_length);
        read_bytes += file_header->name_length;

        for (uint32 i = 0; i < file_header->geometry_count; i++)
        {
            MeshGeometryResourceData* config = &out_resource->geometries[out_resource->geometries.emplace()];
            GeometryResourceData* g = &config->geometry_data;

            check_buffer_size(sizeof(ShmeshFileGeometryHeader));
            ShmeshFileGeometryHeader* geo_header = (ShmeshFileGeometryHeader*)&read_ptr[read_bytes];
            read_bytes += sizeof(*geo_header);

            g->center = geo_header->center;
            g->extents.min = geo_header->min_extents;
            g->extents.max = geo_header->max_extents;
            g->vertex_size = geo_header->vertex_size;
            g->vertex_count = geo_header->vertex_count;
            g->index_count = geo_header->index_count;

            check_buffer_size(
                (geo_header->name_length +
                geo_header->material_name_length +
                (geo_header->vertex_count * geo_header->vertex_size) +
                (geo_header->index_count * geo_header->index_size))
            );

            g->vertices.init(g->vertex_count * g->vertex_size, 0);
            g->indices.init(g->index_count, 0);

            CString::copy((char*)&read_ptr[read_bytes], g->name, Constants::max_geometry_name_length, (int32)geo_header->name_length);
            read_bytes += geo_header->name_length;

            CString::copy((char*)&read_ptr[read_bytes], config->material_name, Constants::max_material_name_length, (int32)geo_header->material_name_length);
            read_bytes += geo_header->material_name_length;

            g->vertices.copy_memory(&read_ptr[read_bytes], geo_header->vertex_count * geo_header->vertex_size, 0);
            read_bytes += geo_header->vertex_count * geo_header->vertex_size;

            g->indices.copy_memory(&read_ptr[read_bytes], g->index_count, 0);
            read_bytes += geo_header->index_count * geo_header->index_size;
        }

        return true;
    }   

    static bool8 vertex3d_equal(Renderer::Vertex3D vert_0, Renderer::Vertex3D vert_1) 
    {
        return 
            (Math::vec_compare(vert_0.position, vert_1.position, Constants::FLOAT_EPSILON) &&
            Math::vec_compare(vert_0.normal, vert_1.normal, Constants::FLOAT_EPSILON) &&
            Math::vec_compare(vert_0.tex_coords, vert_1.tex_coords, Constants::FLOAT_EPSILON) &&
            Math::vec_compare(vert_0.color, vert_1.color, Constants::FLOAT_EPSILON));
    }

    static void reassign_index(uint32 index_count, uint32* indices, uint32 from, uint32 to) 
    {
        for (uint32 i = 0; i < index_count; ++i) 
        {
            if (indices[i] == from)
                indices[i] = to;
            else if (indices[i] > from)
                indices[i]--;
        }
    }

    static void geometry_resource_deduplicate_vertices(GeometryResourceData* geo)
    {

        Darray<Renderer::Vertex3D> new_vertices(geo->vertex_count / 4, 0, (AllocationTag)geo->vertices.allocation_tag);
        Renderer::Vertex3D* old_vertices = (Renderer::Vertex3D*)geo->vertices.transfer_data();        
        uint32 old_vertex_count = geo->vertex_count;

        uint32 found_count = 0;
        for (uint32 o = 0; o < old_vertex_count; o++)
        {
            bool8 found = false;
            for (uint32 n = 0; n < new_vertices.count; n++)
            {
                if (vertex3d_equal(new_vertices[n], old_vertices[o]))
                {
                    reassign_index(geo->index_count, geo->indices.data, o - found_count, n);
                    found = true;
                    found_count++;
                    break;
                }
            }

            if (!found)
            {
                new_vertices.emplace(old_vertices[o]);
            }
        }

        Memory::free_memory(old_vertices);

        geo->vertex_count = new_vertices.count;
        Renderer::Vertex3D* ptr = new_vertices.transfer_data();
        geo->vertices.init(geo->vertex_count * sizeof(Renderer::Vertex3D), 0, (AllocationTag)new_vertices.allocation_tag, ptr);
        
        uint32 removed_count = old_vertex_count - geo->vertex_count;
        SHMDEBUGV("geometry_deduplicate_vertices: removed %u vertices, orig/now %u/%u.", removed_count, old_vertex_count, geo->vertex_count);
    }

}
