#pragma once

#include "ResourceTypes.hpp"
#include "renderer/RendererTypes.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"

#include "systems/FontSystem.hpp"
#include "systems/GeometrySystem.hpp"

struct RenderView;
namespace Renderer
{
	struct RenderViewInstanceData;
}

enum class UITextType
{
	UNKNOWN = 0,
	BITMAP,
	TRUETYPE
};

struct UITextConfig
{
	UITextType type;	
	uint16 font_size;
	const char* font_name;
	const char* text_content;
};

struct UIText
{
	UITextType type;
	ResourceState state;
	bool8 is_dirty;
	UniqueId unique_id;
	uint32 shader_instance_id;
	FontAtlas* font_atlas;
	StringRef text_ref;
	Math::Transform transform;
	GeometryData geometry;
};

SHMAPI bool32 ui_text_init(UITextConfig* config, UIText* out_text);
SHMAPI bool32 ui_text_destroy(UIText* text);
SHMAPI bool32 ui_text_load(UIText* text);
SHMAPI bool32 ui_text_unload(UIText* text);

SHMAPI void ui_text_set_position(UIText* ui_text, Math::Vec3f position);
SHMAPI void ui_text_set_text(UIText* ui_text, const char* text, uint32 offset = 0, uint32 length = Constants::max_u32);
SHMAPI void ui_text_set_text(UIText* ui_text, const String* text, uint32 offset = 0, uint32 length = Constants::max_u32);
SHMAPI void ui_text_update(UIText* ui_text);
