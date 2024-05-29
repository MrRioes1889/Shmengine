#pragma once

#include "../VulkanTypes.hpp"
#include "renderer/RendererTypes.hpp"

bool32 vulkan_object_shader_create(VulkanContext* context, VulkanObjectShader* out_shader);
void vulkan_object_shader_destroy(VulkanContext* context, VulkanObjectShader* shader);

void vulkan_object_shader_use(VulkanContext* context, VulkanObjectShader* shader);