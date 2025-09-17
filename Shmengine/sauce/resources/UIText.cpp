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

static void regenerate_geometry(UIText* ui_text, const FontAtlas* atlas);

static const uint32 quad_vertex_count = 4;
static const uint32 quad_index_count = 6;

bool8 ui_text_init(UITextConfig* config, UIText* out_ui_text)
{
    if (out_ui_text->state >= ResourceState::Initialized)
        return false;

    out_ui_text->state = ResourceState::Initializing;
    out_ui_text->font_id = FontSystem::acquire(config->font_name);
    if (!out_ui_text->font_id.is_valid())
    {
        SHMERROR("Failed to acquire ui text font!");
        return false;
    }
    out_ui_text->font_size = config->font_size;

    out_ui_text->text = config->text_content;
    out_ui_text->transform = Math::transform_create();

    out_ui_text->shader_instance_id.invalidate();

    uint32 text_length = out_ui_text->text.len();
    if (text_length < 1)
        text_length = 1;

    GeometryConfig geometry_config = {};
    geometry_config.extents = {};
    geometry_config.center = {};

    geometry_config.vertex_size = sizeof(Renderer::Vertex2D);
    geometry_config.vertex_count = quad_vertex_count * text_length;
    geometry_config.index_count = text_length * 6;
    Renderer::geometry_init(&geometry_config, &out_ui_text->geometry);

    out_ui_text->unique_id = identifier_acquire_new_id(out_ui_text);
    out_ui_text->is_dirty = true;

    Shader* ui_shader = ShaderSystem::get_shader(ShaderSystem::get_shader_id(Renderer::RendererConfig::builtin_shader_name_ui));
    out_ui_text->shader_instance_id = Renderer::shader_acquire_instance(ui_shader);
    if (!out_ui_text->shader_instance_id.is_valid())
    {
        SHMFATAL("Unable to acquire shader resources for font texture map.");
        return false;
    }

    const FontAtlas* atlas = FontSystem::get_atlas(out_ui_text->font_id, out_ui_text->font_size);
    if (!atlas)
        return false;

    regenerate_geometry(out_ui_text, atlas);
    Renderer::geometry_load(&out_ui_text->geometry);

    out_ui_text->state = ResourceState::Initialized;

    return true;
}

bool8 ui_text_destroy(UIText* ui_text)
{
    if (ui_text->state != ResourceState::Initialized)
        return false;

    ui_text->state = ResourceState::Destroying;

    Renderer::geometry_unload(&ui_text->geometry);

    Shader* ui_shader = ShaderSystem::get_shader(ShaderSystem::get_shader_id(Renderer::RendererConfig::builtin_shader_name_ui));
    Renderer::shader_release_instance(ui_shader, ui_text->shader_instance_id);
    ui_text->shader_instance_id.invalidate();

    identifier_release_id(ui_text->unique_id);
    ui_text->unique_id = 0;


    Renderer::geometry_destroy(&ui_text->geometry);
    ui_text->text.free_data();

    ui_text->state = ResourceState::Destroyed;

    return true;
}

void ui_text_set_position(UIText* ui_text, Math::Vec3f position)
{
    Math::transform_set_position(ui_text->transform, position);
    ui_text->is_dirty = true;
}

void ui_text_set_text(UIText* ui_text, const char* text, uint32 offset, uint32 length)
{   
    SHMASSERT(offset < CString::length(text));
    ui_text->text.copy_n(&text[offset], length);
    ui_text->is_dirty = true;
}

void ui_text_set_text(UIText* ui_text, const String* text, uint32 offset, uint32 length)
{
    ui_text->text.copy_n(&((*text)[offset]), length);
	ui_text->is_dirty = true;
}

bool8 ui_text_set_font(UIText* ui_text, const char* font_name, uint16 font_size)
{
    FontId font_id = FontSystem::acquire(font_name);
    if (!font_id.is_valid())
        return false;

    ui_text->font_id = font_id;
    ui_text->font_size = font_size;
    ui_text->is_dirty = true;
    return true;
}

void ui_text_update(UIText* ui_text)
{
    OPTICK_EVENT();
    if (!ui_text->is_dirty || ui_text->state != ResourceState::Initialized)
        return;

    const FontAtlas* atlas = FontSystem::get_atlas(ui_text->font_id, ui_text->font_size);
    if (!atlas)
        return;

    regenerate_geometry(ui_text, atlas);
	Renderer::geometry_load(&ui_text->geometry);
        
    ui_text->is_dirty = false;
}

static void regenerate_geometry(UIText* ui_text, const FontAtlas* atlas)
{
    OPTICK_EVENT();

    GeometryData* geometry = &ui_text->geometry;

    uint32 max_vertex_count = quad_vertex_count * ui_text->text.len();
    uint32 max_index_count = quad_index_count * ui_text->text.len();

    uint64 vertex_buffer_size = sizeof(Renderer::Vertex2D) * max_vertex_count;
    if (vertex_buffer_size > geometry->vertices.capacity)
    {
        geometry->vertices.resize((uint32)vertex_buffer_size);
        geometry->indices.resize(max_index_count);
    }  

    float32 x = 0;
    float32 y = 0;
    uint32 quad_count = 0;
    SarrayRef<Renderer::Vertex2D> vertices(&geometry->vertices);
    for (uint32 c = 0; c < ui_text->text.len(); c++) 
    {
        char codepoint = ui_text->text[c];

        if (codepoint == '\n') 
        {
            x = 0;
            y += atlas->line_height;
            continue;
        }
        else if (codepoint == '\t') 
        {
            x += atlas->glyphs[' '].x_advance * 4.0f;
            continue;
        }

        const FontGlyph* glyph = &atlas->glyphs[(uint8)codepoint];
        if (!glyph->width)
        {
            x += glyph->x_advance;
            continue;
        }

		float32 minx = x + glyph->x_offset;
		float32 miny = y + glyph->y_offset;
		float32 maxx = minx + glyph->width;
		float32 maxy = miny + glyph->height;
		float32 tminx = (float32)glyph->x / atlas->atlas_size_x;
		float32 tmaxx = (float32)(glyph->x + glyph->width) / atlas->atlas_size_x;
		float32 tminy = (float32)glyph->y / atlas->atlas_size_y;
		float32 tmaxy = (float32)(glyph->y + glyph->height) / atlas->atlas_size_y;
		// Flip the y axis for system text
		if (atlas->type == FontType::Truetype)
		{
			tminy = 1.0f - tminy;
			tmaxy = 1.0f - tmaxy;
		}

		Renderer::Vertex2D p0 = { {minx, miny}, {tminx, tminy} };
		Renderer::Vertex2D p1 = { {maxx, miny}, {tmaxx, tminy} };
		Renderer::Vertex2D p2 = { {maxx, maxy}, {tmaxx, tmaxy} };
		Renderer::Vertex2D p3 = { {minx, maxy}, {tminx, tmaxy} };

		vertices[(quad_count * 4) + 0] = p0;  // 0    3
		vertices[(quad_count * 4) + 1] = p2;  //
		vertices[(quad_count * 4) + 2] = p3;  //
		vertices[(quad_count * 4) + 3] = p1;  // 2    1

		int32 kerning = 0;
		x += glyph->x_advance + kerning;

        geometry->indices[(quad_count * 6) + 0] = (quad_count * 4) + 2;
        geometry->indices[(quad_count * 6) + 1] = (quad_count * 4) + 1;
        geometry->indices[(quad_count * 6) + 2] = (quad_count * 4) + 0;
        geometry->indices[(quad_count * 6) + 3] = (quad_count * 4) + 3;
        geometry->indices[(quad_count * 6) + 4] = (quad_count * 4) + 0;
        geometry->indices[(quad_count * 6) + 5] = (quad_count * 4) + 1;

        quad_count++;
    }

    geometry->vertex_count = quad_vertex_count * quad_count;
    geometry->index_count = quad_index_count * quad_count;
}
