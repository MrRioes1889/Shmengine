#pragma once

#include "Defines.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/TextureSystem.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"
#include "core/Subsystems.hpp"
#include "renderer/RendererTypes.hpp"

struct DirectionalLight;
struct PointLight;
struct LightingInfo;
namespace Renderer
{
	struct RenderViewInstanceData;
}

namespace TextureFilter
{
	enum : uint8
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
	enum : uint8
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

enum class MaterialType : uint8
{
	UNKNOWN,
	PHONG,
	PBR,
	UI,
	CUSTOM
};

namespace MaterialPropertyType
{
	enum : uint8
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
	MaterialPhongProperties materials[Constants::max_terrain_materials_count];
	Math::Vec3f padding;
	uint32 materials_count;
};

struct Material
{
	MaterialId id;
	MaterialType type;
	ShaderId shader_id;
	uint32 shader_instance_id;
	char name[Constants::max_material_name_length];
	
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

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI bool8 load_from_resource(const char* name, const char* resource_name, bool8 auto_destroy);
	SHMAPI bool8 load_from_config(const MaterialConfig* config, bool8 auto_destroy);

	SHMAPI MaterialId acquire_reference(const char* name);
	SHMAPI MaterialId acquire_reference(MaterialId id);
	SHMAPI void release_reference(const char* name);
	SHMAPI void release_reference(MaterialId id);

	SHMAPI Material* get_material(MaterialId id);

	SHMAPI Material* get_default_material();
	SHMAPI Material* get_default_ui_material();

}