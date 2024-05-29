#pragma once

#include "VulkanTypes.hpp"

bool32 create_shader_module(
	VulkanContext* context,
	const char* name,
	const char* type_str,
	VkShaderStageFlagBits stage_flags,
	uint32 stage_index,
	VulkanShaderStage* shader_stages
);
