#include "Terrain.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/RendererTypes.hpp"
#include "renderer/RendererGeometry.hpp"
#include "renderer/RendererFrontend.hpp"
//#include "resources/loaders/TerrainLoader.hpp"

#include "systems/MaterialSystem.hpp"

bool32 terrain_init(TerrainConfig* config, Terrain* out_terrain)
{
    if (out_terrain->state >= TerrainState::INITIALIZED)
        return false;

    if (!config->tile_count_x ||
        !config->tile_count_z ||
        !config->tile_scale_x ||
        !config->tile_scale_z)
    {
        SHMERROR("Failed to init terrain. Tile counts and scales have to be greater than 0.");
        return false;
    }
  
    out_terrain->name = config->name;
    out_terrain->xform = config->xform;

    out_terrain->tile_count_x = config->tile_count_x;
    out_terrain->tile_count_z = config->tile_count_z;
    out_terrain->tile_scale_x = config->tile_scale_x;
    out_terrain->tile_scale_z = config->tile_scale_z;

    out_terrain->pending_materials.init(config->materials_count, DarrayFlags::NON_RESIZABLE);
    out_terrain->materials.init(config->materials_count, DarrayFlags::NON_RESIZABLE);

    Renderer::generate_terrain_geometry(
        out_terrain->tile_count_x, 
        out_terrain->tile_count_z, 
        out_terrain->tile_scale_x, 
        out_terrain->tile_scale_z, 
        out_terrain->name.c_str(), 
        &out_terrain->geometry);

    out_terrain->state = TerrainState::INITIALIZED;

    return true;
}

bool32 terrain_destroy(Terrain* terrain)
{
    if (terrain->state != TerrainState::UNLOADED && !terrain_unload(terrain))
        return false;

    terrain->geometry.vertices.free_data();
    terrain->geometry.indices.free_data();

    terrain->pending_materials.free_data();
    terrain->materials.free_data();

    terrain->name.free_data();

    terrain->state = TerrainState::DESTROYED;
    return true;
}

bool32 terrain_load(Terrain* terrain)
{

    if (terrain->state != TerrainState::INITIALIZED && terrain->state != TerrainState::UNLOADED)
        return false;

    bool32 is_reload = terrain->state == TerrainState::UNLOADED;

    terrain->state = TerrainState::LOADING;

    if (!Renderer::geometry_load(&terrain->geometry))
    {
        SHMERROR("Failed to load terrain geometry!");
        return false;
    }

    terrain->state = TerrainState::LOADED;

    return true;

}

bool32 terrain_unload(Terrain* terrain)
{

    if (terrain->state <= TerrainState::INITIALIZED)
        return true;
    else if (terrain->state != TerrainState::LOADED)
        return false;

    terrain->state = TerrainState::UNLOADING;

    Renderer::geometry_unload(&terrain->geometry);

    terrain->state = TerrainState::UNLOADED;

    return true;

}