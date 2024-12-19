#pragma once

#include "renderer/RendererTypes.hpp"
#include "systems/FontSystem.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"

enum class UITextType
{
	UNKNOWN = 0,
	BITMAP,
	TRUETYPE
};

struct UIText
{
	UITextType type;
	UniqueId unique_id;
	uint32 instance_id;
	FontAtlas* font_atlas;
	Renderer::RenderBuffer vertex_buffer;
	Renderer::RenderBuffer index_buffer;
	String text;
	Math::Transform transform;
	uint64 render_frame_number;
};

SHMAPI bool32 ui_text_create(UITextType type, const char* font_name, uint16 font_size, const char* text_content, UIText* out_text);
SHMAPI void ui_text_destroy(UIText* text);

SHMAPI void ui_text_set_position(UIText* ui_text, Math::Vec3f position);
SHMAPI void ui_text_set_text(UIText* ui_text, const char* text);
SHMAPI void ui_text_refresh(UIText* ui_text);

void ui_text_draw(UIText* ui_text);
