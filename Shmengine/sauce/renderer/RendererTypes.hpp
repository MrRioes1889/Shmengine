#pragma once

#include "Defines.hpp"

#include "core/Identifier.hpp"
#include "platform/Platform.hpp"
#include "containers/Buffer.hpp"
#include "containers/Hashtable.hpp"
#include "utility/String.hpp"
#include "utility/Utility.hpp"
#include "memory/Freelist.hpp"
#include "utility/Math.hpp"
#include "resources/ResourceTypes.hpp"

struct UIText;
struct Skybox;
struct FrameData;
struct Mesh;
struct Material;
struct Texture;
struct TextureMap;
struct Shader;
struct ShaderConfig;
struct ShaderUniform;

typedef AllocationReference32 RenderBufferAllocationReference;

typedef Id16 MaterialId;
typedef Id16 GeometryId;
typedef Id16 ShaderId;
typedef Id16 ShaderUniformId;
typedef Id16 ShaderInstanceId;

namespace Platform
{
	struct PlatformState;
}

namespace Memory
{
	struct LinearAllocator;
}

namespace Renderer
{

	namespace RendererConfigFlags
	{
		typedef uint8 Value;
		enum : Value
		{
			VSYNC = 1 << 0,
			POWER_SAVING = 1 << 1,
		};
	}

	struct SHMAPI RendererConfig
	{
		static inline const char* builtin_shader_name_material_phong = "Builtin.MaterialPhong";
		static inline const char* builtin_shader_name_terrain = "Builtin.Terrain";
		static inline const char* builtin_shader_name_color3D = "Builtin.Color3D";
		static inline const char* builtin_shader_name_coordinate_grid = "Builtin.CoordinateGrid";
		static inline const char* builtin_shader_name_ui = "Builtin.UI";
		static inline const char* builtin_shader_name_skybox = "Builtin.Skybox";
		static inline const char* builtin_shader_name_material_phong_pick = "Builtin.MaterialPhongPick";
		static inline const char* builtin_shader_name_terrain_pick = "Builtin.TerrainPick";
		static inline const char* builtin_shader_name_ui_pick = "Builtin.UIPick";

		static inline const uint32 max_device_name_length = 256;
		inline static const uint32 max_material_count = 0x400;
		inline static const uint32 max_ui_count = 0x400;
		inline static const uint32 max_geometry_count = 0x1000;
		inline static const uint32 framebuffer_count = 3;

		inline static const uint32 shader_max_instance_count = max_material_count;
		inline static const uint32 shader_max_stage_count = 8;
		inline static const uint32 shader_max_global_texture_count = 31;
		inline static const uint32 shader_max_instance_texture_count = 31;
		inline static const uint32 shader_max_attribute_count = 16;
		inline static const uint32 shader_max_uniform_count = 128;
		inline static const uint32 shader_max_binding_count = 2;
		inline static const uint32 shader_max_push_const_range_count = 32;
	};

	struct DeviceProperties
	{
		char device_name[RendererConfig::max_device_name_length];
		uint64 required_ubo_offset_alignment;
	};

	namespace ViewMode
	{
		typedef uint8 Value;
		enum : Value
		{
			DEFAULT = 0,
			LIGHTING = 1,
			NORMALS = 2,
		};
	}

	namespace RenderpassClearFlags
	{
		typedef uint8 Value;
		enum : Value
		{
			NONE = 0,
			COLOR_BUFFER = 1 << 0,
			DEPTH_BUFFER = 1 << 1,
			STENCIL_BUFFER = 1 << 2
		};
	}

	enum class RenderTargetAttachmentType : uint8
	{
		NONE,
		COLOR,
		DEPTH,
		STENCIL
	};

	enum class RenderTargetAttachmentSource : uint8
	{
		NONE,
		DEFAULT,
		VIEW
	};

	enum class RenderTargetAttachmentLoadOp : uint8
	{
		NONE,
		DONT_CARE,
		LOAD
	};

	enum class RenderTargetAttachmentStoreOp : uint8
	{
		NONE,
		DONT_CARE,
		STORE
	};

	struct RenderTargetAttachmentConfig
	{
		RenderTargetAttachmentType type;
		RenderTargetAttachmentSource source;
		RenderTargetAttachmentLoadOp load_op;
		RenderTargetAttachmentStoreOp store_op;
		bool8 present_after;
	};

	struct RenderTargetConfig
	{
		uint32 attachment_count;
		RenderTargetAttachmentConfig* attachment_configs;
	};

	struct RenderTargetAttachment
	{
		RenderTargetAttachmentType type;
		RenderTargetAttachmentSource source;
		RenderTargetAttachmentLoadOp load_op;
		RenderTargetAttachmentStoreOp store_op;
		bool8 present_after;
		Texture* texture;
	};

	struct RenderTarget
	{
		Sarray<RenderTargetAttachment> attachments;
		void* internal_framebuffer;

		bool8 sync_window_to_size;
	};

	enum class RenderCullMode : uint8
	{
		None = 0,
		Front = 1,
		Back = 2,
		Both = 3
	};

	namespace RenderTopologyTypeFlags
	{
		typedef uint8 Value;
		enum : Value
		{
			None = 0,
			TriangleList = 1 << 0,
			TriangleStrip = 1 << 1,
			TriangleFan = 1 << 2,
			LineList = 1 << 3,
			LineStrip = 1 << 4,
			PointList = 1 << 5,
			AllTypesMask = (1 << 6) - 1
		};
	}

	struct RenderPassConfig
	{
		const char* name;

		float32 depth;
		uint32 stencil;

		Math::Vec2u dim;
		Math::Vec2i offset;
		Math::Vec4f clear_color;

		uint32 clear_flags;

		uint32 render_target_count;
		RenderTargetConfig target_config;
	};

	struct RenderPass
	{
		uint32 id;
		uint32 clear_flags;

		String name;

		Math::Vec2u dim;
		Math::Vec2i offset;
		Math::Vec4f clear_color;

		Sarray<RenderTarget> render_targets;
		Buffer internal_data;
	};

	enum class RenderBufferType : uint8
	{
		UNKNOWN,
		VERTEX,
		INDEX,
		UNIFORM,
		STAGING,
		READ,
		STORAGE
	};

	struct RenderBuffer
	{
		String name;
		uint64 size;
		RenderBufferType type;
		bool8 has_freelist;
		Buffer freelist_data;
		Freelist freelist;
		Buffer internal_data;
	};

	struct Vertex3D
	{
		Math::Vec3f position;
		Math::Vec3f normal;
		Math::Vec2f tex_coords;
		Math::Vec4f color;
		Math::Vec3f tangent;
	};

	struct Vertex2D
	{
		Math::Vec2f position;
		Math::Vec2f tex_coordinates;
	};

	struct VertexColor3D
	{
		Math::Vec3f position;
		Math::Vec4f color;
	};

	struct ModuleConfig
	{
		const char* application_name;
	};

	struct Module
	{

		uint32 frame_number;

		uint64(*get_context_size_requirement)();

		bool8(*init)(void* context, const ModuleConfig& config, DeviceProperties* device_properties);
		void (*shutdown)();

		void (*device_sleep_till_idle)();

		void(*on_config_changed)();

		void (*on_resized)(uint32 width, uint32 height);

		bool8(*begin_frame)(const FrameData* frame_data);
		bool8(*end_frame)(const FrameData* frame_data);

		bool8(*render_target_init)(uint32 attachment_count, const RenderTargetAttachment* attachments, RenderPass* pass, uint32 width, uint32 height, RenderTarget* out_target);
		void (*render_target_destroy)(RenderTarget* target, bool8 free_internal_memory);

		bool8(*renderpass_init)(const RenderPassConfig* config, RenderPass* out_renderpass);
		void (*renderpass_destroy)(RenderPass* pass);

		bool8(*renderpass_begin)(RenderPass* pass, RenderTarget* target);
		bool8(*renderpass_end)(RenderPass* pass);

		Texture* (*get_window_attachment)(uint32 index);
		Texture* (*get_depth_attachment)(uint32 attachment_index);
		uint32(*get_window_attachment_index)();
		uint32(*get_window_attachment_count)();

		void (*set_viewport)(Math::Vec4f rect);
		void (*reset_viewport)();

		void (*set_scissor)(Math::Rect2Di rect);
		void (*reset_scissor)();

		bool8 (*texture_init)(Texture* texture);
		void (*texture_destroy)(Texture* texture);
		void (*texture_resize)(Texture* texture, uint32 width, uint32 height);
		bool8(*texture_write_data)(Texture* t, uint32 offset, uint32 size, const uint8* pixels);
		bool8(*texture_read_data)(Texture* t, uint32 offset, uint32 size, void* out_memory);
		bool8(*texture_read_pixel)(Texture* t, uint32 x, uint32 y, uint32* out_rgba);

		bool8(*shader_init)(ShaderConfig* config, Shader* shader);
		void (*shader_destroy)(Shader* shader);
		bool8(*shader_use)(Shader* shader);
		bool8(*shader_bind_globals)(Shader* shader);
		bool8(*shader_bind_instance)(Shader* shader, ShaderInstanceId instance_id);
		bool8(*shader_apply_globals)(Shader* shader);
		bool8(*shader_apply_instance)(Shader* shader);
		bool8(*shader_acquire_instance)(Shader* shader, ShaderInstanceId instance_id);
		bool8(*shader_release_instance)(Shader* shader, ShaderInstanceId instance_id);
		bool8(*shader_set_uniform)(Shader* shader, ShaderUniform* uniform, const void* value);

		bool8(*texture_map_init)(TextureMap* out_map);
		void (*texture_map_destroy)(TextureMap* out_map);

		bool8(*renderbuffer_init)(RenderBuffer* buffer);
		void (*renderbuffer_destroy)(RenderBuffer* buffer);
		bool8(*renderbuffer_bind)(RenderBuffer* buffer, uint64 offset);
		bool8(*renderbuffer_unbind)(RenderBuffer* buffer);
		void* (*renderbuffer_map_memory)(RenderBuffer* buffer, uint64 offset, uint64 size);
		void (*renderbuffer_unmap_memory)(RenderBuffer* buffer);
		bool8(*renderbuffer_flush)(RenderBuffer* buffer, uint64 offset, uint64 size);
		bool8(*renderbuffer_read)(RenderBuffer* buffer, uint64 offset, uint64 size, void* out_memory);
		bool8(*renderbuffer_resize)(RenderBuffer* buffer, uint64 new_total_size);
		bool8(*renderbuffer_load_range)(RenderBuffer* buffer, uint64 offset, uint64 size, const void* data);
		bool8(*renderbuffer_copy_range)(RenderBuffer* source, uint64 source_offset, RenderBuffer* dest, uint64 dest_offset, uint64 size);
		bool8(*renderbuffer_draw)(RenderBuffer* buffer, uint64 offset, uint32 element_count, bool8 bind_only);

		bool8(*is_multithreaded)();
	};

	struct SystemConfig
	{
		const char* application_name;
		RendererConfigFlags::Value flags;
		const char* renderer_module_name;

		uint16 max_shader_uniform_count;
		uint16 max_shader_global_textures;
		uint16 max_shader_instance_textures;
	};

	struct SystemState
	{
		Platform::DynamicLibrary renderer_lib;
		Renderer::Module module;
		void* module_context;

		DeviceProperties device_properties;

		uint16 max_shader_uniform_count;
		uint16 max_shader_global_textures;
		uint16 max_shader_instance_textures;

		RenderBuffer general_vertex_buffer;
		RenderBuffer general_index_buffer;

		uint32 framebuffer_width;
		uint32 framebuffer_height;

		uint32 frames_since_resize;
		bool8 resizing;

		uint8 frame_number;

		RendererConfigFlags::Value flags;
	};
}

struct DirectionalLight
{
	Math::Vec4f color;
	Math::Vec4f direction;
};

struct PointLight 
{
	Math::Vec4f color;
	Math::Vec4f position;
	// Usually 1, make sure denominator never gets smaller than 1
	float32 constant_f;
	// Reduces light intensity linearly
	float32 linear;
	// Makes the light fall off slower at longer distances.
	float32 quadratic;
	float32 padding;
};

struct LightingInfo
{
	DirectionalLight* dir_light;
	uint32 p_lights_count;
	PointLight* p_lights;
};

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
	TextureFilter::Value filter_minify;
	TextureFilter::Value filter_magnify;
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
	ResourceState state;
	MaterialType type;
	ShaderId shader_id;
	ShaderInstanceId shader_instance_id;
	char name[Constants::max_material_name_length];
	
	Sarray<TextureMap> maps;

	uint32 properties_size;
	void* properties;
};

struct GeometryResourceData
{
	char name[Constants::max_geometry_name_length];
	uint32 vertex_size;
	uint32 vertex_count;
	uint32 index_count;

	Math::Vec3f center;
	Math::Extents3D extents;

	Sarray<byte> vertices;	
	Sarray<uint32> indices;
};

enum class GeometryConfigType
{
	Default,
	Cube
};

struct GeometryConfig
{
	struct DefaultConfig 
	{
		uint32 vertex_size;
		uint32 vertex_count;
		uint32 index_count;

		Math::Vec3f center;
		Math::Extents3D extents;

		byte* vertices;	
		uint32* indices;
	};

	struct CubeConfig
	{
		Math::Vec3f dim;
		Math::Vec2f tiling;
	};

	GeometryConfigType type;
	union
	{
		DefaultConfig default_config;
		CubeConfig cube_config;
	};
};

struct GeometryData
{
	Math::Vec3f center;
	Math::Extents3D extents;
	
	bool8 loaded;

	uint32 vertex_size;
	uint32 vertex_count;
	uint32 index_count;

	Sarray<byte> vertices;
	Sarray<uint32> indices;

	RenderBufferAllocationReference vertex_buffer_alloc_ref;
	RenderBufferAllocationReference index_buffer_alloc_ref;
};

struct MeshGeometryConfig
{
	GeometryConfig geo_config;
	const char *material_name;
};

struct MeshGeometry
{
	GeometryData geometry_data;
	MaterialId material_id;
};

struct MeshConfig
{
	uint32 g_configs_count;

	const char* name;
	MeshGeometryConfig* g_configs;
};

struct Mesh
{
	String name;

	ResourceState state;
	UniqueId unique_id;
	Sarray<MeshGeometry> geometries;
	Math::Extents3D extents;
	Math::Vec3f center;
	Math::Transform transform;
};

#define UNIFORM_APPLY_OR_FAIL(expr)                  \
    if ((!expr)) {                                      \
        SHMERRORV("Failed to apply uniform: %s", expr); \
        return false;                                 \
    }

namespace ShaderStage
{
	enum : uint8
	{
		Vertex = 1,
		Geometry = 1 << 1,
		Fragment = 1 << 2,
		Compute = 1 << 3,
	};
	typedef uint8 Value;
}

enum class ShaderAttributeType : uint8
{
	Float32,
	Float32_2,
	Float32_3,
	Float32_4,
	Mat4,
	Int8,
	UInt8,
	Int16,
	UInt16,
	Int32,
	UInt32,
};

enum class ShaderUniformType : uint8
{
	Float32,
	Float32_2,
	Float32_3,
	Float32_4,
	Int8,
	UInt8,
	Int16,
	UInt16,
	Int32,
	UInt32,
	Mat4,
	Sampler,
	Custom = 255
};

enum class ShaderScope : uint8
{
	Global,
	Instance,
	Local
};

enum class ShaderState : uint8
{
	Uninitialized,
	Initializing,
	Initialized
};

struct ShaderAttributeConfig
{
	char name[Constants::max_shader_attribute_name_length];
	uint32 size;
	ShaderAttributeType type;
};

struct ShaderUniformConfig
{
	char name[Constants::max_shader_uniform_name_length];
	uint16 size;
	uint32 location;
	ShaderUniformType type;
	ShaderScope scope;
};

struct ShaderStageConfig
{
	ShaderStage::Value stage;
	char filename[Constants::max_filename_length];
};

struct ShaderConfig
{
	const char* name;
	Renderer::RenderPass* renderpass;

	Renderer::RenderCullMode cull_mode;
	Renderer::RenderTopologyTypeFlags::Value topologies;
	bool8 depth_test;
	bool8 depth_write;

	uint32 stages_count;
	uint32 attributes_count;
	uint32 uniforms_count;

	ShaderAttributeConfig* attributes;
	ShaderUniformConfig* uniforms;
	ShaderStageConfig* stages;
};

struct ShaderUniform
{
	uint32 offset;
	uint16 location;
	ShaderUniformId index;
	uint16 size;
	uint8 set_index;

	ShaderScope scope;
	ShaderUniformType type;
};

struct ShaderAttribute
{
	String name;
	ShaderAttributeType type;
	uint32 size;
};

namespace ShaderFlags
{
	enum : uint8
	{
		DepthTest = 1 << 0,
		DepthWrite = 1 << 1
	};
	typedef uint8 Value;
}

struct ShaderInstance
{
	RenderBufferAllocationReference alloc_ref;
	uint8 last_update_frame_number;
};

struct Shader
{
	String name;

	ShaderFlags::Value shader_flags;
	ResourceState state;
	Renderer::RenderTopologyTypeFlags::Value topologies;
	ShaderScope bound_scope;

	uint8 global_uniform_count;
	uint8 global_uniform_sampler_count;
	uint8 instance_uniform_count;
	uint8 instance_uniform_sampler_count;
	uint8 local_uniform_count;

	uint32 global_ubo_size;
	uint32 global_ubo_stride;
	RenderBufferAllocationReference global_ubo_alloc_ref;

	uint32 ubo_size;
	uint32 instance_ubo_stride;

	uint32 push_constant_size;
	uint32 push_constant_stride;

	Sarray<TextureMap*> global_texture_maps;

	ShaderInstanceId bound_instance_id;
	uint64 bound_ubo_offset;

	HashtableRH<ShaderUniformId, Constants::max_shader_uniform_name_length> uniform_lookup;
	Sarray<ShaderUniform> uniforms;
	Sarray<ShaderAttribute> attributes;

	uint16 attribute_stride;
	uint8 last_update_frame_number;

	uint32 push_constant_range_count;
	Range push_constant_ranges[32];

	uint32 instance_count;
	Sarray<ShaderInstance> instances;
	Sarray<TextureMap*> instance_texture_maps;

	Renderer::RenderBuffer uniform_buffer;

	void* internal_data;
};


struct UIShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId diffuse_texture;
	ShaderUniformId model;

	ShaderUniformId properties;
};

struct MaterialPhongShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId model;
	ShaderUniformId ambient_color;
	ShaderUniformId camera_position;
	ShaderUniformId diffuse_texture;
	ShaderUniformId specular_texture;
	ShaderUniformId normal_texture;
	ShaderUniformId render_mode;
	ShaderUniformId dir_light;
	ShaderUniformId p_lights;
	ShaderUniformId p_lights_count;

	ShaderUniformId properties;
};

struct TerrainShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId model;
	ShaderUniformId ambient_color;
	ShaderUniformId camera_position;
	ShaderUniformId render_mode;
	ShaderUniformId dir_light;
	ShaderUniformId p_lights;
	ShaderUniformId p_lights_count;

	ShaderUniformId properties;
	ShaderUniformId samplers[Constants::max_terrain_materials_count * 3];
};

struct Color3DShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId model;
};

struct CoordinateGridShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId near_clip;
	ShaderUniformId far_clip;
};