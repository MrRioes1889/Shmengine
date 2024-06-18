#pragma once

#include "Defines.hpp"
#include "core/Assert.hpp"
#include "containers/Sarray.hpp"
#include "containers/Buffer.hpp"
#include "utility/Math.hpp"
#include "memory/DynamicAllocator.hpp"

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
	uint32 generations[VulkanConfig::frames_count];
	uint32 ids[VulkanConfig::frames_count];
};

struct VulkanMaterialShader
{

	inline static const uint32 shader_stage_count = 2;
	inline static const uint32 attribute_count = 2;
	inline static const uint32 sampler_count = 1;
	inline static const char* builtin_shader_name = "Builtin.MaterialShader";
	inline static const uint32 descriptor_count = 2;

	struct InstanceState
	{		
		VkDescriptorSet descriptor_sets[VulkanConfig::frames_count];
		VulkanDesriptorState descriptor_states[VulkanMaterialShader::descriptor_count];
	};

	struct GlobalUBO
	{
		Math::Mat4 projection;
		Math::Mat4 view;

		// NOTE: Padding added to comply with some Nvidia standard about uniform object having to be a multiple of 256 bytes large.
		uint8 padding[256 - sizeof(projection) - sizeof(view)];
	};
	static_assert(sizeof(GlobalUBO) % 256 == 0);

	struct InstanceUBO
	{
		Math::Vec4f diffuse_color;

		uint8 padding[256 - sizeof(diffuse_color)];
	};
	static_assert(sizeof(InstanceUBO) % 256 == 0);

	VulkanShaderStage stages[VulkanMaterialShader::shader_stage_count];

	VkDescriptorPool global_descriptor_pool;	
	// NOTE: One descriptor set per frame - max 3 for triple-buffering
	VkDescriptorSet global_descriptor_sets[VulkanConfig::frames_count];
	VkDescriptorSetLayout global_descriptor_set_layout;
	GlobalUBO global_ubo;
	VulkanBuffer global_uniform_buffer;

	VkDescriptorPool object_descriptor_pool;
	VkDescriptorSetLayout object_descriptor_set_layout;
	VulkanBuffer object_uniform_buffer;
	uint32 object_uniform_buffer_index;

	TextureUse sampler_uses[sampler_count];

	InstanceState instance_states[VulkanConfig::max_material_count];

	VulkanPipeline pipeline;
	
};

struct VulkanUIShader
{

	inline static const uint32 shader_stage_count = 2;
	inline static const uint32 attribute_count = 2;
	inline static const uint32 sampler_count = 1;
	inline static const char* builtin_shader_name = "Builtin.UIShader";
	inline static const uint32 descriptor_count = 2;

	struct InstanceState
	{		
		VkDescriptorSet descriptor_sets[VulkanConfig::frames_count];
		VulkanDesriptorState descriptor_states[VulkanUIShader::descriptor_count];
	};

	struct GlobalUBO
	{
		Math::Mat4 projection;
		Math::Mat4 view;

		// NOTE: Padding added to comply with some Nvidia standard about uniform object having to be a multiple of 256 bytes large.
		uint8 padding[256 - sizeof(projection) - sizeof(view)];
	};
	static_assert(sizeof(GlobalUBO) % 256 == 0);

	struct InstanceUBO
	{
		Math::Vec4f diffuse_color;

		uint8 padding[256 - sizeof(diffuse_color)];
	};
	static_assert(sizeof(InstanceUBO) % 256 == 0);

	VulkanShaderStage stages[VulkanUIShader::shader_stage_count];

	VkDescriptorPool global_descriptor_pool;
	// NOTE: One descriptor set per frame - max 3 for triple-buffering
	VkDescriptorSet global_descriptor_sets[VulkanConfig::frames_count];
	VkDescriptorSetLayout global_descriptor_set_layout;
	GlobalUBO global_ubo;
	VulkanBuffer global_uniform_buffer;

	VkDescriptorPool object_descriptor_pool;
	VkDescriptorSetLayout object_descriptor_set_layout;
	VulkanBuffer object_uniform_buffer;
	uint32 object_uniform_buffer_index;

	TextureUse sampler_uses[sampler_count];

	InstanceState instance_states[VulkanConfig::max_ui_count];

	VulkanPipeline pipeline;

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

	VulkanMaterialShader material_shader;
	VulkanUIShader ui_shader;

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