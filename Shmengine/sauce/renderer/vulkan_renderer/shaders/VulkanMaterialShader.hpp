#pragma once

#include "../VulkanTypes.hpp"
#include "renderer/RendererTypes.hpp"

#include "utility/Math.hpp"

bool32 vulkan_material_shader_create(VulkanContext* context, VulkanMaterialShader* out_shader);
void vulkan_material_shader_destroy(VulkanContext* context, VulkanMaterialShader* shader);

void vulkan_material_shader_use(VulkanContext* context, VulkanMaterialShader* shader);

void vulkan_material_shader_update_global_state(VulkanContext* context, VulkanMaterialShader* shader);

void vulkan_material_shader_update_object(VulkanContext* context, VulkanMaterialShader* shader, const Renderer::GeometryRenderData& data);

bool32 vulkan_material_shader_acquire_resources(VulkanContext* context, VulkanMaterialShader* shader, Material* material);
void vulkan_material_shader_release_resources(VulkanContext* context, VulkanMaterialShader* shader, Material* material);