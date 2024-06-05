#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"
#include "containers/Sarray.hpp"
#include "containers/Buffer.hpp"
#include "utility/Math.hpp"

#include "renderer/RendererTypes.hpp"

#include <vulkan/vulkan.h>

#define VK_CHECK(x)						\
	{									\
		SHMASSERT((x) == VK_SUCCESS);		\
	}	

struct VulkanBuffer
{
	VkBuffer handle;
	VkDeviceMemory memory;
	uint64 size;
	bool32 is_locked;	
	VkBufferUsageFlagBits usage;
	int32 memory_index;
	uint32 memory_property_flags;
};

struct VulkanSwapchainSupportInfo
{
	VkSurfaceCapabilitiesKHR capabilities;	
	VkSurfaceFormatKHR* formats;
	VkPresentModeKHR* present_modes;
	uint32 format_count;
	uint32 present_mode_count;
};

struct VulkanDevice
{
	VkPhysicalDevice physical_device;
	VkDevice logical_device;
	VulkanSwapchainSupportInfo swapchain_support;

	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceMemoryProperties memory;

	VkCommandPool graphics_command_pool;

	VkQueue graphics_queue;
	VkQueue present_queue;
	VkQueue transfer_queue;
	int32 graphics_queue_index;
	int32 present_queue_index;
	int32 transfer_queue_index;

	bool32 supports_device_local_host_visible;

	VkFormat depth_format;
};

struct VulkanImage
{
	VkImage handle;
	VkDeviceMemory memory;
	VkImageView view;
	uint32 width;
	uint32 height;
};

enum class VulkanRenderpassState
{
	READY,
	RECORDING,
	IN_RENDER_PASS,
	RECORDING_ENDED,
	SUBMITTED,
	NOT_ALLOCATED
};

struct VulkanRenderpass
{
	VkRenderPass handle;
	Math::Vec2i offset;
	Math::Vec2ui dim;
	Math::Vec4f clear_color;
	float32 depth;
	uint32 stencil;
	VulkanRenderpassState state;
};

struct VulkanFramebuffer
{
	VkFramebuffer handle;
	Sarray<VkImageView> attachments = {};
	VulkanRenderpass* renderpass;
};

struct VulkanSwapchain
{
	VulkanImage depth_attachment;

	Sarray<VulkanFramebuffer> framebuffers = {};

	VkSurfaceFormatKHR image_format;
	VkSwapchainKHR handle;
	Sarray<VkImage> images = {};
	Sarray<VkImageView> views = {};
	uint32 max_frames_in_flight;
};

enum class VulkanCommandBufferState
{
	READY,
	RECORDING,
	IN_RENDER_PASS,
	RECORDING_ENDED,
	SUBMITTED,
	NOT_ALLOCATED
};

struct VulkanCommandBuffer
{
	VkCommandBuffer handle;
	VulkanCommandBufferState state;
};

struct VulkanFence
{
	VkFence handle;
	bool32 signaled;
};

struct VulkanShaderStage
{
	VkShaderModuleCreateInfo module_create_info;
	VkShaderModule handle;
	VkPipelineShaderStageCreateInfo shader_stage_create_info;
	Buffer shader_code_buffer = {};
};

struct VulkanPipeline
{
	VkPipeline handle;
	VkPipelineLayout layout;
};

struct VulkanDesriptorState
{
	uint32 generations[3];
	uint32 ids[3];
};

struct VulkanMaterialShaderObjectState
{	
	inline static const uint32 descriptor_count = 2;
	VkDescriptorSet descriptor_sets[3];
	VulkanDesriptorState descriptor_states[VulkanMaterialShaderObjectState::descriptor_count];
};

struct VulkanMaterialShader
{

	inline static const uint32 shader_stage_count = 2;
	inline static const uint32 attribute_count = 2;
	inline static const uint32 max_object_count = 1024;
	inline static const char* builtin_shader_name = "Builtin.MaterialShader";

	VulkanShaderStage stages[VulkanMaterialShader::shader_stage_count];

	VkDescriptorPool global_descriptor_pool;	
	// NOTE: One descriptor set per frame - max 3 for triple-buffering
	VkDescriptorSet global_descriptor_sets[3];
	VkDescriptorSetLayout global_descriptor_set_layout;
	Renderer::GlobalUniformObject global_ubo;
	VulkanBuffer global_uniform_buffer;

	VkDescriptorPool object_descriptor_pool;
	VkDescriptorSetLayout object_descriptor_set_layout;
	VulkanBuffer object_uniform_buffer;
	uint32 object_uniform_buffer_index;

	VulkanMaterialShaderObjectState object_states[VulkanMaterialShader::max_object_count];

	VulkanPipeline pipeline;
	
};

struct VulkanContext
{
	int32(*find_memory_index)(uint32 type_filter, uint32 property_flags);

	VkInstance instance;
	VkAllocationCallbacks* allocator_callbacks;
	VkSurfaceKHR surface;
	VulkanDevice device;

	VulkanSwapchain swapchain;
	VulkanRenderpass main_renderpass;

	VulkanBuffer object_vertex_buffer;
	VulkanBuffer object_index_buffer;

	uint64 geometry_vertex_offset;
	uint64 geometry_index_offset;

	VulkanMaterialShader object_shader;

	Sarray<VulkanCommandBuffer> graphics_command_buffers = {};

	Sarray<VkSemaphore> image_available_semaphores = {};
	Sarray<VkSemaphore> queue_complete_semaphores = {};

	Sarray<VulkanFence> fences_in_flight = {};
	Sarray<VulkanFence*> images_in_flight = {};

#if defined(_DEBUG)
	VkDebugUtilsMessengerEXT debug_messenger;
#endif

	uint32 image_index;
	uint32 current_frame;
	bool32 recreating_swapchain;

	uint32 framebuffer_size_generation;
	uint32 framebuffer_size_last_generation;

	uint32 framebuffer_width;
	uint32 framebuffer_height;

	float32 frame_delta_time;
};

struct VulkanTextureData
{
	VulkanImage image;
	VkSampler sampler;
};