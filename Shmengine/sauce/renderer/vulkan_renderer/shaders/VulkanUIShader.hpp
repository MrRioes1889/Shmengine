#pragma once

#include "../VulkanTypes.hpp"
#include "renderer/RendererTypes.hpp"

#include "utility/Math.hpp"

namespace Renderer::Vulkan
{
	bool32 ui_shader_create(VulkanContext* context, VulkanUIShader* out_shader);
	void ui_shader_destroy(VulkanContext* context, VulkanUIShader* shader);

	void ui_shader_use(VulkanContext* context, VulkanUIShader* shader);

	void ui_shader_update_global_state(VulkanContext* context, VulkanUIShader* shader);

	void ui_shader_set_model(VulkanContext* context, VulkanUIShader* shader, const Math::Mat4& model);
	void ui_shader_apply_material(VulkanContext* context, VulkanUIShader* shader, Material* material);

	bool32 ui_shader_acquire_resources(VulkanContext* context, VulkanUIShader* shader, Material* material);
	void ui_shader_release_resources(VulkanContext* context, VulkanUIShader* shader, Material* material);
}