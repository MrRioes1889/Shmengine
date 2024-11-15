#pragma once

#include "Defines.hpp"
#include "systems/TextureSystem.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"
#include "core/Subsystems.hpp"

struct DirectionalLight;
struct PointLight;

struct Material
{

	static const uint32 max_name_length = 128;

	uint32 id;
	uint32 generation;
	uint32 internal_id;
	uint32 shader_id;
	char name[max_name_length];
	uint32 render_frame_number;
	float32 shininess;
	Math::Vec4f diffuse_color;
	TextureMap diffuse_map;
	TextureMap specular_map;
	TextureMap normal_map;

};

struct MaterialConfig
{
	char name[Material::max_name_length];
	char diffuse_map_name[Texture::max_name_length];
	char specular_map_name[Texture::max_name_length];
	char normal_map_name[Texture::max_name_length];
	String shader_name;
	Math::Vec4f diffuse_color;
	bool32 auto_release;

	float32 shininess;
};

namespace MaterialSystem
{
	struct SystemConfig
	{
		uint32 max_material_count;

		inline static const char* default_name = "default";
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
	SHMAPI Material* acquire_from_config(const MaterialConfig& config);
	SHMAPI void release(const char* name);

	SHMAPI Material* get_default_material();

	bool32 apply_globals(uint32 shader_id, uint64 renderer_frame_number, const Math::Mat4* projection, const Math::Mat4* view, const Math::Vec4f* ambient_color, const Math::Vec3f* camera_position, uint32 render_mode);
	bool32 apply_instance(Material* m, LightingInfo lighting, bool32 needs_update);
	bool32 apply_local(Material* m, const Math::Mat4& model);

}