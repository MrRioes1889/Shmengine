#include "UIText.hpp"

#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "systems/FontSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "utility/math/Transform.hpp"
#include "renderer/RendererTypes.hpp"
#include "renderer/RendererFrontend.hpp"
#include "containers/Sarray.hpp"

#include "optick.h"

static void regenerate_geometry(UIText* ui_text);

static const uint32 quad_vertex_count = 4;
static const uint32 quad_index_count = 6;

bool32 ui_text_create(UITextType type, const char* font_name, uint16 font_size, const char* text_content, UIText* out_text)
{

    if (out_text->type != UITextType::UNKNOWN)
    {
        SHMERROR("ui_text_create - text object seems to be already initialized!");
        return false;
    }

    out_text->type = type;

    if (!FontSystem::acquire(font_name, font_size, out_text))
    {
        SHMERROR("ui_text_create - failed to acquire resources!");
        return false;
    }

    out_text->text = text_content;
    out_text->transform = Math::transform_create();

    out_text->instance_id = INVALID_ID;
    out_text->render_frame_number = INVALID_ID64;

    Renderer::Shader* ui_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_ui);
    TextureMap* font_maps[1] = { &out_text->font_atlas->map };
    if (!Renderer::shader_acquire_instance_resources(ui_shader, 1, font_maps, &out_text->instance_id))
    {
        SHMFATAL("ui_text_create - Unable to acquire shader resources for font texture map.");
        ui_text_destroy(out_text);
        return false;
    }

    static const uint64 quad_vertex_size = (sizeof(Renderer::Vertex2D) * quad_vertex_count);
    uint32 text_length = out_text->text.len();
    if (text_length < 1)
        text_length = 1;

    char vertex_buffer_name[max_buffer_name_length];
    CString::print_s(vertex_buffer_name, max_buffer_name_length, "%s_%u_%s", font_name, out_text->unique_id, "_v_buf");
    if (!Renderer::renderbuffer_create(vertex_buffer_name, Renderer::RenderBufferType::VERTEX, quad_vertex_size * text_length, true, &out_text->vertex_buffer))
    {
        SHMFATAL("ui_text_create - Failed to create vertex buffer.");
        ui_text_destroy(out_text);
        return false;
    }
    Renderer::renderbuffer_bind(&out_text->vertex_buffer, 0);

    static const uint64 quad_index_size = (sizeof(uint32) * quad_index_count);

    char index_buffer_name[max_buffer_name_length];
    CString::print_s(index_buffer_name, max_buffer_name_length, "%s_%u_%s", font_name, out_text->unique_id, "_i_buf");
    if (!Renderer::renderbuffer_create(index_buffer_name, Renderer::RenderBufferType::INDEX, quad_index_size * text_length, true, &out_text->index_buffer))
    {
        SHMFATAL("ui_text_create - Failed to create index buffer.");
        ui_text_destroy(out_text);
        return false;
    }
    Renderer::renderbuffer_bind(&out_text->index_buffer, 0);

    if (!FontSystem::verify_atlas(out_text->font_atlas, out_text->text.c_str()))
    {
        SHMFATAL("ui_text_create - Failed to verify font atlas.");
        ui_text_destroy(out_text);
        return false;
    }

    regenerate_geometry(out_text);

    out_text->unique_id = identifier_acquire_new_id(out_text);

    return true;

}

void ui_text_destroy(UIText* text)
{

    text->text.free_data();

    Renderer::renderbuffer_destroy(&text->vertex_buffer);
    Renderer::renderbuffer_destroy(&text->index_buffer);

    Renderer::Shader* ui_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_ui);
    Renderer::shader_release_instance_resources(ui_shader, text->instance_id);

    identifier_release_id(text->unique_id);
    text->unique_id = 0;

}

void ui_text_set_position(UIText* ui_text, Math::Vec3f position)
{
    Math::transform_set_position(ui_text->transform, position);
}

void ui_text_set_text(UIText* ui_text, const char* text)
{   
    OPTICK_EVENT();
    if (ui_text->text == text)
        return;  

    ui_text->text = text;
    ui_text_refresh(ui_text);
}

void ui_text_refresh(UIText* ui_text)
{
    if (!FontSystem::verify_atlas(ui_text->font_atlas, ui_text->text.c_str()))
    {
        SHMERROR("ui_text_set_text - font atlas verification failed");
        return;
    }

    regenerate_geometry(ui_text);
}

void ui_text_draw(UIText* ui_text)
{

    uint32 text_length = ui_text->text.len();
    if (text_length > 0)
    {
        if (!Renderer::renderbuffer_draw(&ui_text->vertex_buffer, 0, text_length * quad_vertex_count, true))
        {
            SHMERROR("ui_text_draw - failed to draw vertex renderbuffer");
            return;
        }

        if (!Renderer::renderbuffer_draw(&ui_text->index_buffer, 0, text_length * quad_index_count, false))
        {
            SHMERROR("ui_text_draw - failed to draw index renderbuffer");
            return;
        }
    }
    

}

static void regenerate_geometry(UIText* ui_text)
{

    OPTICK_EVENT();
    uint32 char_length = ui_text->text.len();
    uint32 utf8_length = FontSystem::utf8_string_length(ui_text->text.c_str());

    if (utf8_length < 1)
        return;

    uint32 vertices_count = quad_vertex_count * utf8_length;
    uint32 indices_count = quad_index_count * utf8_length;
    uint64 vertex_buffer_size = sizeof(Renderer::Vertex2D) * vertices_count;
    uint64 index_buffer_size = sizeof(uint32) * indices_count;

    if (vertex_buffer_size > ui_text->vertex_buffer.size)
    {
        if (!Renderer::renderbuffer_resize(&ui_text->vertex_buffer, vertex_buffer_size))
        {
            SHMERROR("regenerate_geometry - Failed to resize vertex buffer.");
            return;
        }
    }

    if (index_buffer_size > ui_text->index_buffer.size)
    {
        if (!Renderer::renderbuffer_resize(&ui_text->index_buffer, index_buffer_size))
        {
            SHMERROR("regenerate_geometry - Failed to resize index buffer.");
            return;
        }
    }

    // Generate new geometry for each character.
    float32 x = 0;
    float32 y = 0;
    // Temp arrays to hold vertex/index data.
    Sarray<Renderer::Vertex2D> vertex_buffer_data(vertices_count, 0);
    Sarray<uint32> index_buffer_data(indices_count, 0);

    // Take the length in chars and get the correct codepoint from it.
    for (uint32 c = 0, uc = 0; c < char_length; ++c) {
        int32 codepoint = ui_text->text[c];

        // Continue to next line for newline.
        if (codepoint == '\n') {
            x = 0;
            y += ui_text->font_atlas->line_height;
            // Increment utf-8 character count.
            uc++;
            continue;
        }

        if (codepoint == '\t') {
            x += ui_text->font_atlas->tab_x_advance;
            uc++;
            continue;
        }

        // NOTE: UTF-8 codepoint handling.
        uint8 advance = 0;
        if (!FontSystem::utf8_bytes_to_codepoint(ui_text->text.c_str(), c, &codepoint, &advance)) {
            SHMWARN("Invalid UTF-8 found in string, using unknown codepoint of -1");
            codepoint = -1;
        }

        FontGlyph* g = 0;
        for (uint32 i = 0; i < ui_text->font_atlas->glyphs.capacity; ++i) {
            if (ui_text->font_atlas->glyphs[i].codepoint == codepoint) {
                g = &ui_text->font_atlas->glyphs[i];
                break;
            }
        }

        if (!g) {
            // If not found, use the codepoint -1
            codepoint = -1;
            for (uint32 i = 0; i < ui_text->font_atlas->glyphs.capacity; ++i) {
                if (ui_text->font_atlas->glyphs[i].codepoint == codepoint) {
                    g = &ui_text->font_atlas->glyphs[i];
                    break;
                }
            }
        }

        if (g) {
            // Found the glyph. generate points.
            float32 minx = x + g->x_offset;
            float32 miny = y + g->y_offset;
            float32 maxx = minx + g->width;
            float32 maxy = miny + g->height;
            float32 tminx = (float32)g->x / ui_text->font_atlas->atlas_size_x;
            float32 tmaxx = (float32)(g->x + g->width) / ui_text->font_atlas->atlas_size_x;
            float32 tminy = (float32)g->y / ui_text->font_atlas->atlas_size_y;
            float32 tmaxy = (float32)(g->y + g->height) / ui_text->font_atlas->atlas_size_y;
            // Flip the y axis for system text
            if (ui_text->type == UITextType::TRUETYPE) {
                tminy = 1.0f - tminy;
                tmaxy = 1.0f - tmaxy;
            }

            Renderer::Vertex2D p0 = { {minx, miny}, {tminx, tminy} };
            Renderer::Vertex2D p1 = { {maxx, miny}, {tmaxx, tminy} };
            Renderer::Vertex2D p2 = { {maxx, maxy}, {tmaxx, tmaxy} };
            Renderer::Vertex2D p3 = { {minx, maxy}, {tminx, tmaxy} };

            vertex_buffer_data[(uc * 4) + 0] = p0;  // 0    3
            vertex_buffer_data[(uc * 4) + 1] = p2;  //
            vertex_buffer_data[(uc * 4) + 2] = p3;  //
            vertex_buffer_data[(uc * 4) + 3] = p1;  // 2    1

            // Try to find kerning
            int32 kerning = 0;

            // Get the offset of the next character. If there is no advance, move forward one,
            // otherwise use advance as-is.
            uint32 offset = c + advance;  //(advance < 1 ? 1 : advance);
            if (offset < utf8_length - 1) {
                // Get the next codepoint.
                int32 next_codepoint = 0;
                uint8 advance_next = 0;

                if (!FontSystem::utf8_bytes_to_codepoint(ui_text->text.c_str(), offset, &next_codepoint, &advance_next)) {
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
            x += g->x_advance + kerning;

        }
        else {
            SHMERROR("Unable to find unknown codepoint. Skipping.");
            // Increment utf-8 character count.
            uc++;
            continue;
        }

        // Index data 210301
        index_buffer_data[(uc * 6) + 0] = (uc * 4) + 2;
        index_buffer_data[(uc * 6) + 1] = (uc * 4) + 1;
        index_buffer_data[(uc * 6) + 2] = (uc * 4) + 0;
        index_buffer_data[(uc * 6) + 3] = (uc * 4) + 3;
        index_buffer_data[(uc * 6) + 4] = (uc * 4) + 0;
        index_buffer_data[(uc * 6) + 5] = (uc * 4) + 1;

        // Now advance c
        c += advance - 1;  // Subtracting 1 because the loop always increments once for single-byte anyway.
        // Increment utf-8 character count.
        uc++;
    }

    if (!Renderer::renderbuffer_load_range(&ui_text->vertex_buffer, 0, vertex_buffer_size, vertex_buffer_data.data))
    {
        SHMERROR("regenerate_geometry - Failed to load vertex buffer data");
        return;
    }

    if (!Renderer::renderbuffer_load_range(&ui_text->index_buffer, 0, index_buffer_size, index_buffer_data.data))
    {
        SHMERROR("regenerate_geometry - Failed to load index buffer data");
        return;
    }

}