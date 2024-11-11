#pragma once

#include "Defines.hpp"

#include "resources/ResourceTypes.hpp"
#include "memory/Freelist.hpp"
#include "utility/Math.hpp"

namespace Platform
{
	struct PlatformState;
}

struct UIText;
struct Skybox;

namespace Memory
{
	struct LinearAllocator;
}

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
		static inline const char* builtin_shader_name_ui = "Shader.Builtin.UI";
		static inline const char* builtin_shader_name_skybox = "Shader.Builtin.Skybox";
		static inline const char* builtin_shader_name_world_pick = "Shader.Builtin.WorldPick";
		static inline const char* builtin_shader_name_ui_pick = "Shader.Builtin.UIPick";

		inline static const uint32 max_material_count = 0x400;
		inline static const uint32 max_ui_count = 0x400;
		inline static const uint32 max_geometry_count = 0x1000;
		inline static const uint32 frames_count = 3;

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

	struct RenderpassConfig
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

	struct Renderpass
	{
		uint32 id;
		uint32 clear_flags;

		Math::Vec2u dim;
		Math::Vec2i offset;
		Math::Vec4f clear_color;

		Sarray<RenderTarget> render_targets;
		Buffer internal_data;
	};

	enum class RenderbufferType
	{
		UNKNOWN,
		VERTEX,
		INDEX,
		UNIFORM,
		STAGING,
		READ,
		STORAGE
	};

	struct Renderbuffer
	{		
		uint64 size;
		RenderbufferType type;
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

		Renderbuffer uniform_buffer;

		void* internal_data;

	};

	struct Vertex3D
	{
		Math::Vec3f position;
		Math::Vec3f normal;
		Math::Vec2f texcoord;
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

		Sarray<RenderpassConfig> pass_configs;
	};

	struct RenderViewPacket;

	struct RenderView
	{
		const char* name;
		uint32 id;
		uint16 width;
		uint16 height;
		RenderViewType type;

		Sarray<Renderpass> renderpasses;

		const char* custom_shader_name;
		Buffer internal_data;

		bool32(*on_create)(RenderView* self);
		void (*on_destroy)(RenderView* self);
		void (*on_resize)(RenderView* self, uint32 width, uint32 height);
		bool32(*on_build_packet)(RenderView* self, Memory::LinearAllocator* frame_allocator, void* data, RenderViewPacket* out_packet);
		void (*on_destroy_packet)(const RenderView* self, RenderViewPacket* packet);
		bool32(*on_render)(RenderView* self, const RenderViewPacket& packet, uint64 frame_number, uint64 render_target_index);
		bool32(*regenerate_attachment_target)(const RenderView* self, uint32 pass_index, RenderTargetAttachment* attachment);
	};

	struct GeometryRenderData
	{
		Math::Mat4 model;
		Geometry* geometry;
		UniqueId unique_id;
	};

	struct RenderViewPacket
	{
		RenderView* view;
		Math::Mat4 view_matrix;
		Math::Mat4 projection_matrix;
		Math::Vec3f view_position;
		Math::Vec4f ambient_color;
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
		GeometryRenderData* geometries;
	};

	struct SkyboxPacketData
	{
		Skybox* skybox;
	};

	struct UIPacketData {
		MeshPacketData mesh_data;
		// TODO: temp
		uint32 text_count;
		UIText** texts;
		// end
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
		float64 delta_time;

		Sarray<RenderViewPacket> views;
	};

	struct ModuleConfig
	{
		const char* application_name;
	};

	struct Module
	{

		uint64 frame_count;

		uint64(*get_context_size_requirement)();

		bool32(*init)(void* context, const ModuleConfig& config, uint32* out_window_render_target_count);
		void (*shutdown)();

		void(*on_config_changed)();

		void (*on_resized)(uint32 width, uint32 height);

		bool32(*begin_frame)(float64 delta_time);
		bool32(*end_frame)(float64 delta_time);

		bool32 (*render_target_create)(uint32 attachment_count, const RenderTargetAttachment* attachments, Renderpass* pass, uint32 width, uint32 height, RenderTarget* out_target);
		void (*render_target_destroy)(RenderTarget* target, bool32 free_internal_memory);

		bool32 (*renderpass_create)(const RenderpassConfig* config, Renderpass* out_renderpass);
		void (*renderpass_destroy)(Renderpass* pass);

		bool32 (*renderpass_begin)(Renderpass* pass, RenderTarget* target);
		bool32 (*renderpass_end)(Renderpass* pass);

		Texture* (*get_window_attachment)(uint32 index);
		Texture* (*get_depth_attachment)(uint32 attachment_index);
		uint32 (*get_window_attachment_index)();	
		uint32 (*get_window_attachment_count)();	

		void (*set_viewport)(Math::Vec4f rect);
		void (*reset_viewport)();

		void (*set_scissor)(Math::Rect2Di rect);
		void (*reset_scissor)();

		void (*geometry_draw)(const GeometryRenderData& data);

		void (*texture_create)(const void* pixels, Texture* texture);
		void (*texture_create_writable)(Texture* texture);
		void (*texture_resize)(Texture* texture, uint32 width, uint32 height);
		bool32 (*texture_write_data)(Texture* t, uint32 offset, uint32 size, const uint8* pixels);
		bool32 (*texture_read_data)(Texture* t, uint32 offset, uint32 size, void* out_memory);
		bool32 (*texture_read_pixel)(Texture* t, uint32 x, uint32 y, uint32* out_rgba);
		void (*texture_destroy)(Texture* texture);	

		bool32(*geometry_create)(Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices);
		void (*geometry_destroy)(Geometry* geometry);

		bool32 (*shader_create)(Shader* shader, const ShaderConfig* config, const Renderpass* renderpass, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages);
		void (*shader_destroy)(Shader* shader);
		bool32 (*shader_init)(Shader* shader);
		bool32 (*shader_use)(Shader* shader);
		bool32 (*shader_bind_globals)(Shader* shader);
		bool32 (*shader_bind_instance)(Shader* shader, uint32 instance_id);
		bool32 (*shader_apply_globals)(Shader* shader);
		bool32 (*shader_apply_instance)(Shader* shader, bool32 needs_update);
		bool32 (*shader_acquire_instance_resources)(Shader* shader, TextureMap** maps, uint32* out_instance_id);
		bool32 (*shader_release_instance_resources)(Shader* shader, uint32 instance_id);
		bool32 (*shader_set_uniform)(Shader* shader, ShaderUniform* uniform, const void* value);
		bool32 (*texture_map_acquire_resources)(TextureMap* out_map);
		void (*texture_map_release_resources)(TextureMap* out_map);

		bool32 (*renderbuffer_create_internal)(Renderbuffer* buffer);
		void (*renderbuffer_destroy_internal)(Renderbuffer* buffer);
		bool32 (*renderbuffer_bind)(Renderbuffer* buffer, uint64 offset);
		bool32 (*renderbuffer_unbind)(Renderbuffer* buffer);
		void* (*renderbuffer_map_memory)(Renderbuffer* buffer, uint64 offset, uint64 size);
		void (*renderbuffer_unmap_memory)(Renderbuffer* buffer);
		bool32 (*renderbuffer_flush)(Renderbuffer* buffer, uint64 offset, uint64 size);
		bool32 (*renderbuffer_read)(Renderbuffer* buffer, uint64 offset, uint64 size, void* out_memory);
		bool32 (*renderbuffer_resize)(Renderbuffer* buffer, uint64 new_total_size);
		bool32 (*renderbuffer_load_range)(Renderbuffer* buffer, uint64 offset, uint64 size, const void* data);
		bool32 (*renderbuffer_copy_range)(Renderbuffer* source, uint64 source_offset, Renderbuffer* dest, uint64 dest_offset, uint64 size);
		bool32 (*renderbuffer_draw)(Renderbuffer* buffer, uint64 offset, uint32 element_count, bool32 bind_only);

		bool8 (*is_multithreaded)();
	};

}
