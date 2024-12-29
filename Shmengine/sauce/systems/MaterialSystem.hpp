#pragma once

#include "Defines.hpp"
#include "systems/TextureSystem.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"
#include "core/Subsystems.hpp"

struct DirectionalLight;
struct PointLight;

namespace TextureFilter
{
	enum
	{
		NEAREST = 0,
		LINEAR = 1,
		FILTER_TYPES_COUNT
	};
	typedef uint8 Value;
}

static const char* texture_filter_names[TextureFilter::FILTER_TYPES_COUNT] =
{
	"nearest",
	"linear"
};

namespace TextureRepeat
{
	enum
	{
		REPEAT = 0,
		MIRRORED_REPEAT = 1,
		CLAMP_TO_EDGE = 2,
		CLAMP_TO_BORDER = 3,
		REPEAT_TYPES_COUNT
	};
	typedef uint8 Value;
}

static const char* texture_repeat_names[TextureRepeat::REPEAT_TYPES_COUNT] =
{
	"repeat",
	"mirrored_repeat",
	"clamp_to_edge",
	"clamp_to_border"
};

struct TextureMapConfig
{
	const char* name;
	const char* texture_name;
	TextureFilter::Value filter_min;
	TextureFilter::Value filter_mag;
	TextureRepeat::Value repeat_u;
	TextureRepeat::Value repeat_v;
	TextureRepeat::Value repeat_w;
};

struct TextureMap
{
	void* internal_data;
	Texture* texture;

	TextureFilter::Value filter_minify;
	TextureFilter::Value filter_magnify;

	TextureRepeat::Value repeat_u;
	TextureRepeat::Value repeat_v;
	TextureRepeat::Value repeat_w;
};

enum class MaterialType
{
	UNKNOWN,
	PHONG,
	PBR,
	UI,
	TERRAIN,
	CUSTOM
};

namespace MaterialPropertyType
{
	enum
	{
		INVALID,
		UINT8,
		INT8,
		UINT16,
		INT16,
		UINT32,
		INT32,
		FLOAT32,
		UINT64,
		INT64,
		FLOAT64,
		FLOAT32_2,
		FLOAT32_3,
		FLOAT32_4,
		FLOAT32_16,
		PROPERTY_TYPE_COUNT
	};
	typedef uint32 Value;
}

static const uint32 material_property_type_sizes[MaterialPropertyType::PROPERTY_TYPE_COUNT] =
{
	0,
	1,
	1,
	2,
	2,
	4,
	4,
	4,
	8,
	8,
	8,
	4 * 2,
	4 * 3,
	4 * 4,
	4 * 16
};

struct MaterialProperty
{
	static const uint32 max_name_length = 64;
	char name[max_name_length];
	MaterialPropertyType::Value type;
	union
	{
		uint8 u8[64];
		int8 i8[64];
		uint16 u16[32];
		int16 i16[32];
		uint32 u32[16];
		int32 i32[16];
		float32 f32[16];
		uint64 u64[8];
		int64 i64[8];
		float64 f64[8];
	};
};

struct MaterialConfig
{
	const char* name;
	const char* shader_name;

	MaterialType type;
	bool32 auto_release;

	MaterialProperty* properties;
	uint32 properties_count;

	uint32 maps_count;
	TextureMapConfig* maps;
};

struct MaterialPhongProperties
{
	Math::Vec4f diffuse_color;
	Math::Vec3f padding;
	float32 shininess;
};

struct MaterialUIProperties
{
	Math::Vec4f diffuse_color;
};

struct MaterialTerrainProperties
{
	MaterialPhongProperties materials[max_terrain_materials_count];
	Math::Vec3f padding;
	uint32 materials_count;
	Math::Vec4f padding2;
};

struct Material
{

	uint32 id;
	MaterialType type;
	uint32 generation;
	uint32 internal_id;
	uint32 shader_id;
	uint32 render_frame_number;
	char name[max_material_name_length];
	
	Sarray<TextureMap> maps;

	uint32 properties_size;
	void* properties;

};

namespace MaterialSystem
{
	struct SystemConfig
	{
		uint32 max_material_count;

		inline static const char* default_name = "default";
		inline static const char* default_ui_name = "default_ui";
		inline static const char* default_terrain_name = "default_terrain";
	};

	struct LightingInfo
	{
		DirectionalLight* dir_light;
		uint32 p_lights_count;
		PointLight* p_lights;
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI Material* acquire(const char* name);
	SHMAPI Material* acquire_from_config(const MaterialConfig* config);

	SHMAPI Material* acquire_terrain_material(const char* name, uint32 material_count, const char** material_names, bool32 auto_release);

	SHMAPI void release(const char* name);

	SHMAPI Material* get_default_material();
	SHMAPI Material* get_default_ui_material();
	SHMAPI Material* get_default_terrain_material();

	bool32 apply_globals(uint32 shader_id, LightingInfo lighting, uint64 renderer_frame_number, const Math::Mat4* projection, const Math::Mat4* view, const Math::Vec4f* ambient_color, const Math::Vec3f* camera_position, uint32 render_mode);
	bool32 apply_instance(Material* m, LightingInfo lighting, bool32 needs_update);
	bool32 apply_local(Material* m, const Math::Mat4& model);

	void dump_system_stats();

}