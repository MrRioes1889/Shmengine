#pragma once

#include "renderer/RendererTypes.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"

#include "systems/FontSystem.hpp"
#include "systems/GeometrySystem.hpp"

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
	uint32 shader_instance_id;
	uint32 render_frame_number;
	FontAtlas* font_atlas;
	String text;
	Math::Transform transform;
	GeometryData geometry;
};

SHMAPI bool32 ui_text_create(UITextType type, const char* font_name, uint16 font_size, const char* text_content, UIText* out_text);
SHMAPI void ui_text_destroy(UIText* text);

SHMAPI void ui_text_set_position(UIText* ui_text, Math::Vec3f position);
SHMAPI void ui_text_set_text(UIText* ui_text, const char* text);
SHMAPI void ui_text_refresh(UIText* ui_text);

SHMAPI bool32 ui_text_on_render(uint32 shader_id, LightingInfo lighting, Math::Mat4* model, void* text, uint32 frame_number);
