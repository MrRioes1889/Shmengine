#include "UIText.hpp"

#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "core/FrameData.hpp"
#include "systems/FontSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/RenderViewSystem.hpp"
#include "utility/math/Transform.hpp"
#include "renderer/RendererTypes.hpp"
#include "renderer/RendererFrontend.hpp"
#include "containers/Sarray.hpp"

#include "optick.h"

static void regenerate_geometry(UIText* ui_text);

static const uint32 quad_vertex_count = 4;
static const uint32 quad_index_count = 6;

bool32 ui_text_init(UITextConfig* config, UIText* out_ui_text)
{

    if (out_ui_text->state >= ResourceState::Initialized)
        return false;

    out_ui_text->state = ResourceState::Initializing;

    out_ui_text->type = config->type;

    if (!FontSystem::acquire(config->font_name, config->font_size, out_ui_text))
    {
        SHMERROR("ui_text_create - failed to acquire resources!");
        return false;
    }

    out_ui_text->text = config->text_content;
    out_ui_text->text_ref = config->text_content;
    out_ui_text->transform = Math::transform_create();

    out_ui_text->shader_instance_id = Constants::max_u32;

    uint32 text_length = out_ui_text->text.len();
    if (text_length < 1)
        text_length = 1;

    GeometryData* geometry = &out_ui_text->geometry;
    geometry->extents = {};
    geometry->center = {};

    geometry->vertex_size = sizeof(Renderer::Vertex2D);
    geometry->vertex_count = quad_vertex_count * text_length;
    geometry->vertices.init(geometry->vertex_size * geometry->vertex_count, 0);

    geometry->index_count = text_length * 6;
    geometry->indices.init(geometry->index_count, 0);

    out_ui_text->unique_id = identifier_acquire_new_id(out_ui_text);

    out_ui_text->is_dirty = true;
    out_ui_text->state = ResourceState::Initialized;

    return true;

}

bool32 ui_text_destroy(UIText* ui_text)
{
    if (ui_text->state != ResourceState::Unloaded && !ui_text_unload(ui_text))
        return false;

    ui_text->text.free_data();

    ui_text->geometry.vertices.free_data();
    ui_text->geometry.indices.free_data();

    ui_text->state = ResourceState::Destroyed;

    return true;
}

bool32 ui_text_load(UIText* ui_text)
{
    if (ui_text->state != ResourceState::Initialized && ui_text->state != ResourceState::Unloaded)
        return false;

    ui_text->state = ResourceState::Loading;

    Shader* ui_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_ui);
    if (!Renderer::shader_acquire_instance_resources(ui_shader, 1, &ui_text->shader_instance_id))
    {
        SHMFATAL("Unable to acquire shader resources for font texture map.");
        return false;
    }

    regenerate_geometry(ui_text);
    Renderer::geometry_load(&ui_text->geometry);

    ui_text->state = ResourceState::Loaded;

    return true;
}

bool32 ui_text_unload(UIText* ui_text)
{
    if (ui_text->state <= ResourceState::Initialized)
        return true;
    else if (ui_text->state != ResourceState::Loaded)
        return false;

    ui_text->state = ResourceState::Unloading;

    Renderer::geometry_unload(&ui_text->geometry);

    Shader* ui_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_ui);
    Renderer::shader_release_instance_resources(ui_shader, ui_text->shader_instance_id);

    ui_text->shader_instance_id = Constants::max_u32;

    identifier_release_id(ui_text->unique_id);
    ui_text->unique_id = 0;

    ui_text->state = ResourceState::Unloaded;
    return true;
}

void ui_text_set_position(UIText* ui_text, Math::Vec3f position)
{
    Math::transform_set_position(ui_text->transform, position);
    ui_text->is_dirty = true;
}

void ui_text_set_text(UIText* ui_text, const char* text, uint32 offset, int32 length)
{   
    ui_text->text_ref.set_text(text, offset, length);
    if (ui_text->text == text)
        return;  

    if (length >= 0)
        ui_text->text.copy_n(text, length);
    else
        ui_text->text = text;

    ui_text->is_dirty = true;
}

void ui_text_set_text(UIText* ui_text, const String* text, uint32 offset, int32 length)
{   
    ui_text->text_ref.set_text(text, offset, length);
	ui_text->is_dirty = true;
}

void ui_text_update(UIText* ui_text)
{
    if (!ui_text->is_dirty)
        return;

    if (!FontSystem::verify_atlas(ui_text->font_atlas, ui_text->text.c_str()))
    {
        SHMERROR("Font atlas verification failed");
        return;
    }

    uint64 old_vertex_buffer_size = ui_text->geometry.vertices.size();
    uint64 old_index_buffer_size = ui_text->geometry.indices.size();

    regenerate_geometry(ui_text);

    if (ui_text->state == ResourceState::Loaded)
        Renderer::geometry_reload(&ui_text->geometry, old_vertex_buffer_size, old_index_buffer_size);
        
    ui_text->is_dirty = false;
}

static void regenerate_geometry(UIText* ui_text)
{

    OPTICK_EVENT();

    uint32 char_length = ui_text->text_ref.len();
    uint32 utf8_length = FontSystem::utf8_string_length(ui_text->text_ref.c_str(), ui_text->text_ref.len(), true);
    const char* test = ui_text->text_ref.c_str();
    uint32 utf8_length_alt = FontSystem::utf8_string_length(ui_text->text.c_str(), ui_text->text.len(), true);

    if (utf8_length < 1)
        return;

    GeometryData* geometry = &ui_text->geometry;

    geometry->vertex_count = quad_vertex_count * utf8_length;
    geometry->index_count = quad_index_count * utf8_length;
    uint64 vertex_buffer_size = sizeof(Renderer::Vertex2D) * geometry->vertex_count;

    if (vertex_buffer_size > geometry->vertices.capacity)
    {
        geometry->vertices.resize((uint32)vertex_buffer_size);
        geometry->indices.resize(geometry->index_count);
    }  

    // Generate new geometry for each character.
    float32 x = 0;
    float32 y = 0;

    // Take the length in chars and get the correct codepoint from it.
    for (uint32 c = 0, uc = 0; c < char_length; ++c) {
        int32 codepoint = ui_text->text_ref[c];

        // Continue to next line for newline.
        if (codepoint == '\n') {
            x = 0;
            y += ui_text->font_atlas->line_height;
            continue;
        }

        if (codepoint == '\t') {
            x += ui_text->font_atlas->tab_x_advance;
            continue;
        }

        // NOTE: UTF-8 codepoint handling.
        uint8 advance = 0;
        if (!FontSystem::utf8_bytes_to_codepoint(ui_text->text_ref.c_str(), c, &codepoint, &advance)) {
            SHMWARN("Invalid UTF-8 found in string, using unknown codepoint of -1");
            codepoint = -1;
        }

        FontGlyph* glyph = 0;
        for (uint32 i = 0; i < ui_text->font_atlas->glyphs.capacity; ++i) {
            if (ui_text->font_atlas->glyphs[i].codepoint == codepoint) {
                glyph = &ui_text->font_atlas->glyphs[i];
                break;
            }
        }

        if (!glyph) {
            // If not found, use the codepoint -1
            codepoint = -1;
            for (uint32 i = 0; i < ui_text->font_atlas->glyphs.capacity; ++i) {
                if (ui_text->font_atlas->glyphs[i].codepoint == codepoint) {
                    glyph = &ui_text->font_atlas->glyphs[i];
                    break;
                }
            }
        }

        if (glyph) {
            // Found the glyph. generate points.
            float32 minx = x + glyph->x_offset;
            float32 miny = y + glyph->y_offset;
            float32 maxx = minx + glyph->width;
            float32 maxy = miny + glyph->height;
            float32 tminx = (float32)glyph->x / ui_text->font_atlas->atlas_size_x;
            float32 tmaxx = (float32)(glyph->x + glyph->width) / ui_text->font_atlas->atlas_size_x;
            float32 tminy = (float32)glyph->y / ui_text->font_atlas->atlas_size_y;
            float32 tmaxy = (float32)(glyph->y + glyph->height) / ui_text->font_atlas->atlas_size_y;
            // Flip the y axis for system text
            if (ui_text->type == UITextType::TRUETYPE) {
                tminy = 1.0f - tminy;
                tmaxy = 1.0f - tmaxy;
            }

            Renderer::Vertex2D p0 = { {minx, miny}, {tminx, tminy} };
            Renderer::Vertex2D p1 = { {maxx, miny}, {tmaxx, tminy} };
            Renderer::Vertex2D p2 = { {maxx, maxy}, {tmaxx, tmaxy} };
            Renderer::Vertex2D p3 = { {minx, maxy}, {tminx, tmaxy} };

            geometry->vertices.get_as<Renderer::Vertex2D>((uc * 4) + 0) = p0;  // 0    3
            geometry->vertices.get_as<Renderer::Vertex2D>((uc * 4) + 1) = p2;  //
            geometry->vertices.get_as<Renderer::Vertex2D>((uc * 4) + 2) = p3;  //
            geometry->vertices.get_as<Renderer::Vertex2D>((uc * 4) + 3) = p1;  // 2    1

            // Try to find kerning
            int32 kerning = 0;

            // Get the offset of the next character. If there is no advance, move forward one,
            // otherwise use advance as-is.
            uint32 offset = c + advance;  //(advance < 1 ? 1 : advance);
            if (offset < utf8_length - 1) {
                // Get the next codepoint.
                int32 next_codepoint = 0;
                uint8 advance_next = 0;

                if (!FontSystem::utf8_bytes_to_codepoint(ui_text->text_ref.c_str(), offset, &next_codepoint, &advance_next)) {
                    SHMWARN("Invalid UTF-8 found in string, using unknown codepoint of -1");
                    codepoint = -1;
                }
                else {
                    for (uint32 i = 0; i < ui_text->font_atlas->kernings.count; ++i) {
                        FontKerning* k = &ui_text->font_atlas->kernings[i];
                        if (k->codepoint_0 == codepoint && k->codepoint_1 == next_codepoint) {
                            kerning = k->advance;
                        }
                    }
                }
            }
            x += glyph->x_advance + kerning;

        }
        else {
            SHMERROR("Unable to find unknown codepoint. Skipping.");
            // Increment utf-8 character count.
            uc++;
            continue;
        }

        geometry->indices[(uc * 6) + 0] = (uc * 4) + 2;
        geometry->indices[(uc * 6) + 1] = (uc * 4) + 1;
        geometry->indices[(uc * 6) + 2] = (uc * 4) + 0;
        geometry->indices[(uc * 6) + 3] = (uc * 4) + 3;
        geometry->indices[(uc * 6) + 4] = (uc * 4) + 0;
        geometry->indices[(uc * 6) + 5] = (uc * 4) + 1;

        // Now advance c
        c += advance - 1;  // Subtracting 1 because the loop always increments once for single-byte anyway.
        // Increment utf-8 character count.
        uc++;
    }

}
