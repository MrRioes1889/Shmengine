#pragma once

#include "Defines.hpp"

#include "resources/ResourceTypes.hpp"
#include "utility/Math.hpp"

namespace Platform
{
	struct PlatformState;
}

namespace Renderer
{

	struct RendererConfig
	{
		static inline const char* builtin_shader_name_material = "Shader.Builtin.Material";
		static inline const char* builtin_shader_name_ui = "Shader.Builtin.UI";
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
		enum
		{
			NONE = 0,
			COLOR_BUFFER = 1 << 0,
			DEPTH_BUFFER = 1 << 1,
			STENCIL_BUFFER = 1 << 2
		};
	}

	struct RenderTarget
	{
		Sarray<Texture*> attachments;
		void* internal_framebuffer;

		bool32 sync_window_to_size;
	};

	struct RenderpassConfig
	{
		const char* name;
		const char* prev_name;
		const char* next_name;

		Math::Vec2u dim;
		Math::Vec2i offset;
		Math::Vec4f clear_color;

		uint32 clear_flags;
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

	enum BackendType
	{
		VULKAN,
		OPENGL,
		DIRECTX,
	};

	struct BackendConfig
	{
		const char* application_name;
		uint32 pass_config_count;
		RenderpassConfig* pass_configs;

		void (*on_render_target_refresh_required)();
	};

	struct GeometryRenderData
	{
		Math::Mat4 model;
		Geometry* geometry;
	};

	struct Vertex3D
	{
		Math::Vec3f position;
		Math::Vec3f normal;
		Math::Vec2f texcoord;
		Math::Vec4f color;
		Math::Vec4f tangent;
	};

	struct Vertex2D
	{
		Math::Vec2f position;
		Math::Vec2f tex_coordinates;
	};

	struct Backend
	{

		uint64 frame_count;

		bool32(*init)(const BackendConfig& config, uint32* out_window_render_target_count);
		void (*shutdown)();

		void (*on_resized)(uint32 width, uint32 height);

		bool32(*begin_frame)(float32 delta_time);
		bool32(*end_frame)(float32 delta_time);

		void (*render_target_create)(uint32 attachment_count, Texture* const * attachments, Renderpass* pass, uint32 width, uint32 height, RenderTarget* out_target);
		void (*render_target_destroy)(RenderTarget* target, bool32 free_internal_memory);

		void (*renderpass_create)(Renderpass* out_renderpass, float32 depth, uint32 stencil, bool32 has_prev_pass, bool32 has_next_pass);
		void (*renderpass_destroy)(Renderpass* pass);
		Renderpass* (*renderpass_get)(const char* name);

		bool32(*renderpass_begin)(Renderpass* pass, RenderTarget* target);
		bool32(*renderpass_end)(Renderpass* pass);

		Texture* (*window_attachment_get)(uint32 index);
		Texture* (*depth_attachment_get)();
		uint32 (*window_attachment_index_get)();		

		void (*geometry_draw)(const GeometryRenderData& data);

		void (*texture_create)(const void* pixels, Texture* texture);
		void (*texture_create_writable)(Texture* texture);
		void (*texture_resize)(Texture* texture, uint32 width, uint32 height);
		void (*texture_write_data)(Texture* texture, uint32 offset, uint32 size, const uint8* pixels);
		void (*texture_destroy)(Texture* texture);

		bool32(*geometry_create)(Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices);
		void (*geometry_destroy)(Geometry* geometry);

		bool32(*shader_create)(Shader* shader, Renderpass* renderpass, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages);
		void(*shader_destroy)(Shader* shader);
		bool32(*shader_init)(Shader* shader);
		bool32(*shader_use)(Shader* shader);
		bool32(*shader_bind_globals)(Shader* shader);
		bool32(*shader_bind_instance)(Shader* shader, uint32 instance_id);
		bool32(*shader_apply_globals)(Shader* shader);
		bool32(*shader_apply_instance)(Shader* shader, bool32 needs_update);
		bool32(*shader_acquire_instance_resources)(Shader* shader, TextureMap** maps, uint32* out_instance_id);
		bool32(*shader_release_instance_resources)(Shader* shader, uint32 instance_id);
		bool32(*shader_set_uniform)(Shader* shader, ShaderUniform* uniform, const void* value);
		bool32(*texture_map_acquire_resources)(TextureMap* out_map);
		void(*texture_map_release_resources)(TextureMap* out_map);
	};

	struct RenderData
	{
		float32 delta_time;

		Darray<GeometryRenderData> world_geometries;
		Darray<GeometryRenderData> ui_geometries;
	};
}
