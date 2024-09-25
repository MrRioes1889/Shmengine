#pragma once

#include "Defines.hpp"
#include "resources/ResourceTypes.hpp"

namespace MaterialSystem
{
	struct Config
	{
		uint32 max_material_count;

		inline static const char* default_name = "default";
	};

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config);
	void system_shutdown();

	SHMAPI Material* acquire(const char* name);
	SHMAPI Material* acquire_from_config(const MaterialConfig& config);
	SHMAPI void release(const char* name);

	SHMAPI Material* get_default_material();

	bool32 apply_globals(uint32 shader_id, uint64 renderer_frame_number, const Math::Mat4* projection, const Math::Mat4* view, const Math::Vec4f* ambient_color, const Math::Vec3f* camera_position, uint32 render_mode);
	bool32 apply_instance(Material* m, bool32 needs_update);
	bool32 apply_local(Material* m, const Math::Mat4& model);


}