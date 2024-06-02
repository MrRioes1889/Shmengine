#pragma once

#include "../VulkanTypes.hpp"
#include "renderer/RendererTypes.hpp"

#include "utility/Math.hpp"

bool32 vulkan_object_shader_create(VulkanContext* context, VulkanObjectShader* out_shader);
void vulkan_object_shader_destroy(VulkanContext* context, VulkanObjectShader* shader);

void vulkan_object_shader_use(VulkanContext* context, VulkanObjectShader* shader);

void vulkan_object_shader_update_global_state(VulkanContext* context, VulkanObjectShader* shader);

void vulkan_object_shader_update_object(VulkanContext* context, VulkanObjectShader* shader, Math::Mat4 model);