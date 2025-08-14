#pragma once

#include "Defines.hpp"

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

namespace Platform
{
	struct PlatformState;
}

namespace Memory
{
	struct LinearAllocator;
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

typedef Id16 MaterialId;
typedef Id16 GeometryId;

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

		inline static const uint32 max_material_count = 0x400;
		inline static const uint32 max_ui_count = 0x400;
		inline static const uint32 max_geometry_count = 0x1000;
		inline static const uint32 framebuffer_count = 3;

		inline static const uint32 shader_max_instances = max_material_count;
		inline static const uint32 shader_max_stages = 8;
		inline static const uint32 shader_max_global_textures = 31;
		inline static const uint32 shader_max_instance_textures = 31;
		inline static const uint32 shader_max_attributes = 16;
		inline static const uint32 shader_max_uniforms = 128;
		inline static const uint32 shader_max_bindings = 2;
		inline static const uint32 shader_max_push_const_ranges = 32;
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

		bool8(*init)(void* context, const ModuleConfig& config, uint32* out_window_render_target_count);
		void (*shutdown)();

		void (*device_sleep_till_idle)();

		void(*on_config_changed)();

		void (*on_resized)(uint32 width, uint32 height);

		bool8(*begin_frame)(const FrameData* frame_data);
		bool8(*end_frame)(const FrameData* frame_data);

		bool8(*render_target_create)(uint32 attachment_count, const RenderTargetAttachment* attachments, RenderPass* pass, uint32 width, uint32 height, RenderTarget* out_target);
		void (*render_target_destroy)(RenderTarget* target, bool8 free_internal_memory);

		bool8(*renderpass_create)(const RenderPassConfig* config, RenderPass* out_renderpass);
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

		bool8 (*texture_create)(Texture* texture);
		void (*texture_resize)(Texture* texture, uint32 width, uint32 height);
		bool8(*texture_write_data)(Texture* t, uint32 offset, uint32 size, const uint8* pixels);
		bool8(*texture_read_data)(Texture* t, uint32 offset, uint32 size, void* out_memory);
		bool8(*texture_read_pixel)(Texture* t, uint32 x, uint32 y, uint32* out_rgba);
		void (*texture_destroy)(Texture* texture);

		bool8(*shader_create)(const ShaderConfig* config, Shader* shader);
		void (*shader_destroy)(Shader* shader);
		bool8(*shader_init)(Shader* shader);
		bool8(*shader_use)(Shader* shader);
		bool8(*shader_bind_globals)(Shader* shader);
		bool8(*shader_bind_instance)(Shader* shader, uint32 instance_id);
		bool8(*shader_apply_globals)(Shader* shader);
		bool8(*shader_apply_instance)(Shader* shader);
		bool8(*shader_acquire_instance_resources)(Shader* shader, uint32 texture_maps_count, uint32 instance_id);
		bool8(*shader_release_instance_resources)(Shader* shader, uint32 instance_id);
		bool8(*shader_set_uniform)(Shader* shader, ShaderUniform* uniform, const void* value);
		bool8(*texture_map_acquire_resources)(TextureMap* out_map);
		void (*texture_map_release_resources)(TextureMap* out_map);

		bool8(*renderbuffer_create_internal)(RenderBuffer* buffer);
		void (*renderbuffer_destroy_internal)(RenderBuffer* buffer);
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

	struct SystemState
	{
		Platform::DynamicLibrary renderer_lib;
		Renderer::Module module;
		void* module_context;

		RenderBuffer general_vertex_buffer;
		RenderBuffer general_index_buffer;

		uint32 framebuffer_width;
		uint32 framebuffer_height;

		uint32 window_render_target_count;

		bool8 resizing;
		uint32 frames_since_resize;

		uint8 frame_number;

		RendererConfigFlags::Value flags;
	};
}