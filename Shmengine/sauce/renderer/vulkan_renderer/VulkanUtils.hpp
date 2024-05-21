#pragma once

#include "VulkanTypes.hpp"

const char* vulkan_result_string(VkResult result, bool32 get_extended);

bool32 vulkan_result_is_success(VkResult result);
