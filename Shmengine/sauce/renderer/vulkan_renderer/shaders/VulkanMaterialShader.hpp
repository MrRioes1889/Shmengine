#pragma once

#include "../VulkanTypes.hpp"
#include "renderer/RendererTypes.hpp"

#include "utility/Math.hpp"

namespace Renderer::Vulkan
{
	bool32 material_shader_create(VulkanContext* context, VulkanMaterialShader* out_shader);
	void material_shader_destroy(VulkanContext* context, VulkanMaterialShader* shader);

	void material_shader_use(VulkanContext* context, VulkanMaterialShader* shader);

	void material_shader_update_global_state(VulkanContext* context, VulkanMaterialShader* shader);

	void material_shader_set_model(VulkanContext* context, VulkanMaterialShader* shader, const Math::Mat4& model);
	void material_shader_apply_material(VulkanContext* context, VulkanMaterialShader* shader, Material* material);

	bool32 material_shader_acquire_resources(VulkanContext* context, VulkanMaterialShader* shader, Material* material);
	void material_shader_release_resources(VulkanContext* context, VulkanMaterialShader* shader, Material* material);
}