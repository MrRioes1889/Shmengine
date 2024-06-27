#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"
#include "containers/Sarray.hpp"
#include "containers/Buffer.hpp"
#include "utility/Math.hpp"
#include "memory/DynamicAllocator.hpp"
#include "containers/Hashtable.hpp"

#include "renderer/RendererTypes.hpp"

#include <vulkan/vulkan.h>

#define VK_CHECK(x)						\
	{									\
		SHMASSERT((x) == VK_SUCCESS);		\
	}	

struct VulkanConfig
{
	inline static const uint32 max_material_count = 0x400;
	inline static const uint32 max_ui_count = 0x400;
	inline static const uint32 max_geometry_count = 0x1000;	
	inline static const uint32 frames_count = 3;

	inline static const uint32 shader_max_instances = max_material_count;
	inline static const uint32 shader_max_stages = 8;
	inline static const uint32 shader_max_global_textures = 31;
	inline static const uint32 shader_max_instance_textures = 31;
	inline static const uint32 shader_max_attributes = 16;
	inline static const uint32 shader_max_uniforms = 128;
	inline static const uint32 shader_max_bindings = 2;
	inline static const uint32 shader_max_push_const_ranges = 32;
};

struct VulkanBuffer
{
	VkBuffer handle;
	VkDeviceMemory memory;
	uint64 size;
	bool32 is_locked;	
	VkBufferUsageFlagBits usage;
	int32 memory_index;
	uint32 memory_property_flags;

	Buffer freelist_data;
	Freelist freelist;
	bool32 has_freelist;
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
	float32 depth;
	uint32 stencil;
	Math::Vec2i offset;
	Math::Vec2ui dim;
	Math::Vec4f clear_color;
	VulkanRenderpassState state;
	uint32 clear_flags;
	bool32 has_prev_pass;
	bool32 has_next_pass;
};

struct VulkanSwapchain
{
	VulkanImage depth_attachment;

	VkFramebuffer framebuffers[VulkanConfig::frames_count];

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

struct VulkanShaderStageConfig
{
	static const uint32 max_filename_length = 255;
	VkShaderStageFlagBits stage;
	char filename[max_filename_length];
};

struct VulkanDescriptorSetConfig
{
	uint32 binding_count;
	VkDescriptorSetLayoutBinding bindings[VulkanConfig::shader_max_bindings];
};

struct VulkanShaderStage
{
	VkShaderModuleCreateInfo module_create_info;
	VkShaderModule handle;
	VkPipelineShaderStageCreateInfo shader_stage_create_info;
};

struct VulkanShaderConfig
{
	uint32 stage_count;
	uint16 max_descriptor_set_count;
	uint16 descriptor_set_count;

	VulkanShaderStageConfig stages[VulkanConfig::shader_max_stages];

	VkDescriptorPoolSize pool_sizes[2];

	VulkanDescriptorSetConfig descriptor_sets[2];

	VkVertexInputAttributeDescription attributes[VulkanConfig::shader_max_attributes];
};

struct VulkanDescriptorState
{
	uint8 generations[VulkanConfig::frames_count];
	uint32 ids[VulkanConfig::frames_count];
};

struct VulkanShaderDescriptorSetState
{
	VkDescriptorSet descriptor_sets[VulkanConfig::frames_count];
	VulkanDescriptorState descriptor_states[VulkanConfig::shader_max_bindings];
};

struct VulkanShaderInstanceState
{
	uint32 id;
	uint64 offset;

	VulkanShaderDescriptorSetState descriptor_set_state;
	struct Texture** instance_textures;

};

struct VulkanPipeline
{
	VkPipeline handle;
	VkPipelineLayout layout;
};

struct VulkanShader
{

	uint32 id;

	VulkanShaderConfig config;

	VulkanRenderpass* renderpass;

	VulkanShaderStage stages[VulkanConfig::shader_max_stages];

	VkDescriptorPool descriptor_pool;

	VkDescriptorSetLayout descriptor_set_layouts[2];

	VkDescriptorSet global_descriptor_sets[VulkanConfig::frames_count];

	void* mapped_uniform_buffer;
	VulkanBuffer uniform_buffer;

	VulkanPipeline pipeline;

	// TODO: Make dynamic array
	uint32 instance_count;
	VulkanShaderInstanceState instance_states[VulkanConfig::shader_max_instances];

};

struct VulkanGeometryData
{
	uint32 id;
	uint32 generation;
	uint32 vertex_count;
	uint32 vertex_size;
	uint64 vertex_buffer_offset;
	uint32 index_count;
	uint32 index_size;
	uint64 index_buffer_offset;
};

struct VulkanContext
{
	int32(*find_memory_index)(uint32 type_filter, uint32 property_flags);

	VkInstance instance;
	VkAllocationCallbacks* allocator_callbacks;
	VkSurfaceKHR surface;
	VulkanDevice device;

	VulkanSwapchain swapchain;
	VulkanRenderpass world_renderpass;
	VulkanRenderpass ui_renderpass;

	VulkanBuffer object_vertex_buffer;
	VulkanBuffer object_index_buffer;

	VkFramebuffer world_framebuffers[VulkanConfig::frames_count];

	VulkanGeometryData geometries[VulkanConfig::max_geometry_count];

	Sarray<VulkanCommandBuffer> graphics_command_buffers = {};

	Sarray<VkSemaphore> image_available_semaphores = {};
	Sarray<VkSemaphore> queue_complete_semaphores = {};

	VkFence fences_in_flight[VulkanConfig::frames_count - 1];
	VkFence images_in_flight[VulkanConfig::frames_count];

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