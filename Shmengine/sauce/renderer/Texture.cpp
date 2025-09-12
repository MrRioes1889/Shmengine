#include "renderer/RendererFrontend.hpp"
#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "resources/loaders/TextureLoader.hpp"
#include "utility/math/Transform.hpp"

#include "systems/JobSystem.hpp"
#include "systems/ShaderSystem.hpp"

namespace Renderer
{
	extern SystemState* system_state;

	static bool8 _texture_init(TextureConfig* config, Texture* out_texture);
	static void _texture_destroy(Texture* texture);

	static void _texture_init_from_resource_job_success(void* params);
	static void _texture_init_from_resource_job_fail(void* params);
	static bool8 _texture_init_from_resource_job(uint32 thread_index, void* user_data);

	bool8 texture_init(Texture* texture)
	{
		 return system_state->module.texture_init(texture);
	}

	void texture_resize(Texture* texture, uint32 width, uint32 height)
	{
		system_state->module.texture_resize(texture, width, height);
	}

	bool8 texture_write_data(Texture* t, uint32 offset, uint32 size, const uint8* pixels)
	{
		return system_state->module.texture_write_data(t, offset, size, pixels);
	}

	bool8 texture_read_data(Texture* t, uint32 offset, uint32 size, void* out_memory)
	{
		return system_state->module.texture_read_data(t, offset, size, out_memory);
	}

	bool8 texture_read_pixel(Texture* t, uint32 x, uint32 y, uint32* out_rgba)
	{
		return system_state->module.texture_read_pixel(t, x, y, out_rgba);
	}

	void texture_destroy(Texture* texture)
	{
		system_state->module.texture_destroy(texture);
		texture->internal_data.free_data();
	}
}
