#pragma once

#include "ResourceTypes.hpp"
#include "renderer/RendererTypes.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"

#include "systems/FontSystem.hpp"

struct RenderView;
namespace Renderer
{
	struct RenderViewInstanceData;
}

struct UITextConfig
{
	const char* text_content;
	const char* font_name;
	uint16 font_size;
};

struct UIText
{
	ResourceState state;
	bool8 is_dirty;
	UniqueId unique_id;
	FontId font_id;
	uint16 font_size;
	uint32 shader_instance_id;
	String text;
	Math::Transform transform;
	GeometryData geometry;
};

SHMAPI bool8 ui_text_init(UITextConfig* config, UIText* out_text);
SHMAPI bool8 ui_text_destroy(UIText* text);

SHMAPI void ui_text_set_position(UIText* ui_text, Math::Vec3f position);
SHMAPI void ui_text_set_text(UIText* ui_text, const char* text, uint32 offset = 0, uint32 length = Constants::max_u32);
SHMAPI void ui_text_set_text(UIText* ui_text, const String* text, uint32 offset = 0, uint32 length = Constants::max_u32);
SHMAPI bool8 ui_text_set_font(UIText* ui_text, const char* font_name, uint16 font_size);
SHMAPI void ui_text_update(UIText* ui_text);
