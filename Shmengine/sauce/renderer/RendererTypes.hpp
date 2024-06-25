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

	enum BackendType
	{
		VULKAN,
		OPENGL,
		DIRECTX,
	};

	struct GeometryRenderData
	{
		Math::Mat4 model;
		Geometry* geometry;
	};

	namespace BuiltinRenderpassType
	{
		enum Value
		{
			WORLD = 1,
			UI = 2
		};
	}

	struct Vertex3D
	{
		Math::Vec3f position;
		Math::Vec3f normal;
		Math::Vec2f tex_coordinates;
	};

	struct Vertex2D
	{
		Math::Vec2f position;
		Math::Vec2f tex_coordinates;
	};

	struct Backend
	{

		uint64 frame_count;

		bool32(*init)(const char* application_name);
		void (*shutdown)();

		void (*on_resized)(uint32 width, uint32 height);

		bool32(*begin_frame)(float32 delta_time);
		bool32(*end_frame)(float32 delta_time);

		bool32 (*begin_renderpass)(uint32 renderpass_id);
		bool32 (*end_renderpass)(uint32 renderpass_id);

		void (*draw_geometry)(const GeometryRenderData& data);

		void (*create_texture)(const void* pixels, Texture* texture);
		void (*destroy_texture)(Texture* texture);

		bool32(*create_geometry)(Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices);
		void (*destroy_geometry)(Geometry* geometry);

		bool32(*shader_create)(Shader* shader, uint8 renderpass_id, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages);
		void(*shader_destroy)(Shader* shader);
		bool32(*shader_init)(Shader* shader);
		bool32(*shader_use)(Shader* shader);
		bool32(*shader_bind_globals)(Shader* shader);
		bool32(*shader_bind_instance)(Shader* shader, uint32 instance_id);
		bool32(*shader_apply_globals)(Shader* shader);
		bool32(*shader_apply_instance)(Shader* shader);
		bool32(*shader_acquire_instance_resources)(Shader* shader, uint32* out_instance_id);
		bool32(*shader_release_instance_resources)(Shader* shader, uint32 instance_id);
		bool32(*shader_set_uniform)(Shader* shader, ShaderUniform* uniform, const void* value);
	};

	struct RenderData
	{
		float32 delta_time;

		uint32 world_geometry_count;
		GeometryRenderData* world_geometries;

		uint32 ui_geometry_count;
		GeometryRenderData* ui_geometries;
	};
}
