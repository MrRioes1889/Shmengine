#pragma once

#include "Defines.hpp"

#include "containers/Buffer.hpp"
#include "containers/Hashtable.hpp"
#include "utility/String.hpp"
#include "utility/Utility.hpp"
#include "memory/Freelist.hpp"
#include "utility/Math.hpp"

struct UIText;
struct Skybox;
struct FrameData;
struct Mesh;
struct GeometryData;
struct Material;
struct Texture;
struct TextureMap;

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

namespace Renderer
{

	namespace RendererConfigFlags
	{
		enum
		{
			VSYNC = 1 << 0,
			POWER_SAVING = 1 << 1,
		};
		typedef uint32 Value;
	}

	struct SHMAPI RendererConfig
	{
		static inline const char* builtin_shader_name_material = "Shader.Builtin.Material";
		static inline const char* builtin_shader_name_terrain = "Shader.Builtin.Terrain";
		static inline const char* builtin_shader_name_ui = "Shader.Builtin.UI";
		static inline const char* builtin_shader_name_skybox = "Shader.Builtin.Skybox";
		static inline const char* builtin_shader_name_world_pick = "Shader.Builtin.WorldPick";
		static inline const char* builtin_shader_name_ui_pick = "Shader.Builtin.UIPick";

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
		enum Value
		{
			DEFAULT = 0,
			LIGHTING = 1,
			NORMALS = 2,
		};
	}

	namespace RenderpassClearFlags
	{
		enum Value
		{
			NONE = 0,
			COLOR_BUFFER = 1 << 0,
			DEPTH_BUFFER = 1 << 1,
			STENCIL_BUFFER = 1 << 2
		};
	}

	enum class RenderTargetAttachmentType
	{
		NONE,
		COLOR,
		DEPTH,
		STENCIL
	};

	enum class RenderTargetAttachmentSource
	{
		NONE,
		DEFAULT,
		VIEW
	};

	enum class RenderTargetAttachmentLoadOp
	{
		NONE,
		DONT_CARE,
		LOAD
	};

	enum class RenderTargetAttachmentStoreOp
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
		bool32 present_after;
	};

	struct RenderTargetConfig
	{
		Sarray<RenderTargetAttachmentConfig> attachment_configs;
	};

	struct RenderTargetAttachment
	{
		RenderTargetAttachmentType type;
		RenderTargetAttachmentSource source;
		RenderTargetAttachmentLoadOp load_op;
		RenderTargetAttachmentStoreOp store_op;
		bool32 present_after;
		Texture* texture;
	};

	struct RenderTarget
	{
		Sarray<RenderTargetAttachment> attachments;
		void* internal_framebuffer;

		bool32 sync_window_to_size;
	};

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

	enum class RenderBufferType
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
		bool32 has_freelist;
		Buffer freelist_data;
		Freelist freelist;
		Buffer internal_data;
	};

	namespace ShaderStage
	{
		enum Value
		{
			VERTEX = 1,
			GEOMETRY = 1 << 1,
			FRAGMENT = 1 << 2,
			COMPUTE = 1 << 3,
		};
	}

	enum class ShaderFaceCullMode
	{
		NONE = 0,
		FRONT = 1,
		BACK = 2,
		BOTH = 3
	};

	enum class ShaderAttributeType
	{
		FLOAT32,
		FLOAT32_2,
		FLOAT32_3,
		FLOAT32_4,
		MAT4,
		INT8,
		UINT8,
		INT16,
		UINT16,
		INT32,
		UINT32,
	};

	enum class ShaderUniformType
	{
		FLOAT32,
		FLOAT32_2,
		FLOAT32_3,
		FLOAT32_4,
		INT8,
		UINT8,
		INT16,
		UINT16,
		INT32,
		UINT32,
		MAT4,
		SAMPLER,
		CUSTOM = 255
	};

	enum class ShaderScope
	{
		GLOBAL,
		INSTANCE,
		LOCAL
	};

	struct ShaderAttributeConfig
	{
		String name;
		uint32 size;
		ShaderAttributeType type;
	};

	struct ShaderUniformConfig
	{
		String name;
		uint32 size;
		uint32 location;
		ShaderUniformType type;
		ShaderScope scope;
	};

	struct ShaderConfig
	{
		String name;

		Darray<ShaderAttributeConfig> attributes;
		Darray<ShaderUniformConfig> uniforms;
		Darray<ShaderStage::Value> stages;
		Darray<String> stage_names;
		Darray<String> stage_filenames;

		bool8 depth_test;
		bool8 depth_write;

		ShaderFaceCullMode cull_mode;
	};

	enum class ShaderState
	{
		NOT_CREATED,
		UNINITIALIZED,
		INITIALIZED
	};

	struct ShaderUniform
	{
		uint32 offset;
		uint16 location;
		uint16 index;
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
		enum
		{
			DEPTH_TEST = 1 << 0,
			DEPTH_WRITE = 1 << 1
		};
		typedef uint32 Value;
	}

	struct Shader
	{
		uint32 id;
		ShaderFlags::Value shader_flags;

		uint64 required_ubo_alignment;
		uint32 global_ubo_size;
		uint32 global_ubo_stride;
		uint64 global_ubo_offset;

		uint32 ubo_size;
		uint32 ubo_stride;

		uint32 push_constant_size;
		uint32 push_constant_stride;

		String name;

		Darray<TextureMap*> global_texture_maps;

		ShaderScope bound_scope;

		uint32 bound_instance_id;
		uint64 bound_ubo_offset;

		ShaderState state;
		uint32 instance_texture_count;

		Hashtable<uint16> uniform_lookup;
		Darray<ShaderUniform> uniforms;
		Darray<ShaderAttribute> attributes;

		uint16 attribute_stride;
		uint32 push_constant_range_count;
		Range push_constant_ranges[32];

		uint64 renderer_frame_number;

		RenderBuffer uniform_buffer;

		void* internal_data;

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

	enum class RenderViewType
	{
		WORLD = 1,
		UI = 2,
		SKYBOX = 3,
		PICK = 4
	};

	enum class RenderViewViewMatrixSource
	{
		SCENE_CAMERA = 1,
		UI_CAMERA = 2,
		LIGHT_CAMERA = 3
	};

	enum class RenderViewProjMatrixSource
	{
		DEFAULT_PERSPECTIVE = 1,
		DEFAULT_ORTHOGRAPHIC = 2
	};


	struct RenderViewConfig
	{
		const char* name;
		const char* custom_shader_name;

		uint16 width;
		uint16 height;
		RenderViewType type;
		RenderViewViewMatrixSource view_matrix_source;
		RenderViewProjMatrixSource proj_matrix_source;

		Sarray<RenderPassConfig> pass_configs;
	};

	struct RenderViewPacket;

	struct RenderView
	{
		const char* name;
		uint32 id;
		uint16 width;
		uint16 height;
		RenderViewType type;

		Sarray<RenderPass> renderpasses;

		const char* custom_shader_name;
		Buffer internal_data;

		bool32(*on_create)(RenderView* self);
		void (*on_destroy)(RenderView* self);
		void (*on_resize)(RenderView* self, uint32 width, uint32 height);
		bool32(*on_build_packet)(RenderView* self, Memory::LinearAllocator* frame_allocator, void* data, RenderViewPacket* out_packet);
		void (*on_destroy_packet)(const RenderView* self, RenderViewPacket* packet);
		bool32(*on_render)(RenderView* self, RenderViewPacket& packet, uint32 frame_number, uint64 render_target_index);
		bool32(*regenerate_attachment_target)(const RenderView* self, uint32 pass_index, RenderTargetAttachment* attachment);
	};

	

	struct GeometryRenderData
	{		
		/*UniqueId unique_id;
		uint32 shader_instance_id;
		uint32 texture_maps_count;
		GeometryData* geometry;
		void* properties;
		TextureMap* texture_maps;
		uint32* render_frame_number;
		Math::Mat4 model;*/

		UniqueId unique_id;
		uint32 shader_id;	
		void* render_object;
		bool32(*on_render)(uint32 shader_id, LightingInfo lighting, Math::Mat4* model, void* render_object, uint32 frame_number);
		GeometryData* geometry_data;
		Math::Mat4 model;	
		bool8 has_transparency;	
	};

	struct RenderViewPacket
	{
		RenderView* view;
		Math::Mat4* view_matrix;
		Math::Mat4* projection_matrix;
		Math::Vec3f view_position;
		Math::Vec4f ambient_color;
		LightingInfo lighting;
		Darray<GeometryRenderData> geometries;
		const char* custom_shader_name;
		void* extended_data;
	};

	struct MeshPacketData
	{
		uint32 mesh_count;
		Mesh** meshes;
	};

	struct WorldPacketData
	{
		uint32 geometries_count;
		uint32 p_lights_count;

		GeometryRenderData* geometries;
		DirectionalLight* dir_light;
		PointLight* p_lights;
	};

	struct SkyboxPacketData
	{
		uint32 geometries_count;
		GeometryRenderData* geometries;
	};

	struct UIPacketData {
		uint32 geometries_count;

		GeometryRenderData* geometries;
	};

	struct PickPacketData {
		uint32 world_geometries_count;
		uint32 ui_geometries_count;
		uint32 text_count;

		MeshPacketData ui_mesh_data;

		GeometryRenderData* world_geometries;
		// TODO: temp

		UIText** texts;
		// end
	};

	struct RenderPacket
	{
		Sarray<RenderViewPacket> views;
	};

	struct ModuleConfig
	{
		const char* application_name;
	};

	struct Module
	{

		uint32 frame_number;

		uint64(*get_context_size_requirement)();

		bool32(*init)(void* context, const ModuleConfig& config, uint32* out_window_render_target_count);
		void (*shutdown)();

		void (*device_sleep_till_idle)();

		void(*on_config_changed)();

		void (*on_resized)(uint32 width, uint32 height);

		bool32(*begin_frame)(const FrameData* frame_data);
		bool32(*end_frame)(const FrameData* frame_data);

		bool32(*render_target_create)(uint32 attachment_count, const RenderTargetAttachment* attachments, RenderPass* pass, uint32 width, uint32 height, RenderTarget* out_target);
		void (*render_target_destroy)(RenderTarget* target, bool32 free_internal_memory);

		bool32(*renderpass_create)(const RenderPassConfig* config, RenderPass* out_renderpass);
		void (*renderpass_destroy)(RenderPass* pass);

		bool32(*renderpass_begin)(RenderPass* pass, RenderTarget* target);
		bool32(*renderpass_end)(RenderPass* pass);

		Texture* (*get_window_attachment)(uint32 index);
		Texture* (*get_depth_attachment)(uint32 attachment_index);
		uint32(*get_window_attachment_index)();
		uint32(*get_window_attachment_count)();

		void (*set_viewport)(Math::Vec4f rect);
		void (*reset_viewport)();

		void (*set_scissor)(Math::Rect2Di rect);
		void (*reset_scissor)();

		void (*texture_create)(const void* pixels, Texture* texture);
		void (*texture_create_writable)(Texture* texture);
		void (*texture_resize)(Texture* texture, uint32 width, uint32 height);
		bool32(*texture_write_data)(Texture* t, uint32 offset, uint32 size, const uint8* pixels);
		bool32(*texture_read_data)(Texture* t, uint32 offset, uint32 size, void* out_memory);
		bool32(*texture_read_pixel)(Texture* t, uint32 x, uint32 y, uint32* out_rgba);
		void (*texture_destroy)(Texture* texture);

		bool32(*shader_create)(Shader* shader, const ShaderConfig* config, const RenderPass* renderpass, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages);
		void (*shader_destroy)(Shader* shader);
		bool32(*shader_init)(Shader* shader);
		bool32(*shader_use)(Shader* shader);
		bool32(*shader_bind_globals)(Shader* shader);
		bool32(*shader_bind_instance)(Shader* shader, uint32 instance_id);
		bool32(*shader_apply_globals)(Shader* shader);
		bool32(*shader_apply_instance)(Shader* shader, bool32 needs_update);
		bool32(*shader_acquire_instance_resources)(Shader* shader, uint32 maps_count, TextureMap** maps, uint32* out_instance_id);
		bool32(*shader_release_instance_resources)(Shader* shader, uint32 instance_id);
		bool32(*shader_set_uniform)(Shader* shader, ShaderUniform* uniform, const void* value);
		bool32(*texture_map_acquire_resources)(TextureMap* out_map);
		void (*texture_map_release_resources)(TextureMap* out_map);

		bool32(*renderbuffer_create_internal)(RenderBuffer* buffer);
		void (*renderbuffer_destroy_internal)(RenderBuffer* buffer);
		bool32(*renderbuffer_bind)(RenderBuffer* buffer, uint64 offset);
		bool32(*renderbuffer_unbind)(RenderBuffer* buffer);
		void* (*renderbuffer_map_memory)(RenderBuffer* buffer, uint64 offset, uint64 size);
		void (*renderbuffer_unmap_memory)(RenderBuffer* buffer);
		bool32(*renderbuffer_flush)(RenderBuffer* buffer, uint64 offset, uint64 size);
		bool32(*renderbuffer_read)(RenderBuffer* buffer, uint64 offset, uint64 size, void* out_memory);
		bool32(*renderbuffer_resize)(RenderBuffer* buffer, uint64 new_total_size);
		bool32(*renderbuffer_load_range)(RenderBuffer* buffer, uint64 offset, uint64 size, const void* data);
		bool32(*renderbuffer_copy_range)(RenderBuffer* source, uint64 source_offset, RenderBuffer* dest, uint64 dest_offset, uint64 size);
		bool32(*renderbuffer_draw)(RenderBuffer* buffer, uint64 offset, uint32 element_count, bool32 bind_only);

		bool8(*is_multithreaded)();
	};

}