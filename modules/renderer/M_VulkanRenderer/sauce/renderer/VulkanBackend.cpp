#include "VulkanBackend.hpp"

#include "VulkanTypes.hpp"
#include "VulkanInternal.hpp"
#include "platform/VulkanPlatform.hpp"

#include <core/Logging.hpp>
#include <core/Event.hpp>
#include <containers/Darray.hpp>
#include <utility/CString.hpp>
#include <utility/Math.hpp>

#include <systems/ShaderSystem.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/TextureSystem.hpp>

// TODO: Get rid of frontend include
#include <renderer/RendererFrontend.hpp>
#include <optick.h>

#define VULKAN_USE_CUSTOM_ALLOCATOR 1

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data
);

namespace Renderer::Vulkan
{

	static void create_command_buffers();
	static bool8 recreate_swapchain();
	static int32 find_memory_index(uint32 type_filter, uint32 property_flags);
	static void process_task(TaskInfo task);
	static void create_vulkan_allocator(VkAllocationCallbacks*& callbacks);

	VulkanContext* context = 0;

	bool8 init(void* context_block, const ModuleConfig& config, uint32* out_window_render_target_count)
	{

		context = (VulkanContext*)context_block;
		context->find_memory_index = find_memory_index;

		context->is_multithreaded = false;
		context->config_changed = false;

		create_vulkan_allocator(context->allocator_callbacks);

		context->framebuffer_width = 1600;
		context->framebuffer_height = 900;

		VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app_info.apiVersion = VK_API_VERSION_1_3;
		app_info.pApplicationName = config.application_name;
		app_info.applicationVersion = VK_MAKE_VERSION(0,0,1);
		app_info.pEngineName = "Shmengine";
		app_info.engineVersion = VK_MAKE_VERSION(0,0,1);

		VkInstanceCreateInfo inst_create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		inst_create_info.pApplicationInfo = &app_info;

		const uint32 max_extension_count = 10;
		uint32 extension_count = 1;
		const char* extension_names[max_extension_count] =
		{
			VK_KHR_SURFACE_EXTENSION_NAME
		};

#if defined(_WIN32)		
		extension_names[extension_count] = "VK_KHR_win32_surface";;
		extension_count++;
#endif

#if defined(_DEBUG)

		extension_names[extension_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
		SHMDEBUG(extension_names[extension_count]);
		extension_count++;

#endif

		SHMDEBUG("Required vulkan extensions:");

		for (uint32 i = 0; i < extension_count; i++)
			SHMDEBUG(extension_names[i]);

		uint32 available_extension_count = 0;
		VK_CHECK(vkEnumerateInstanceExtensionProperties(0, &available_extension_count, 0));
		Sarray<VkExtensionProperties> available_extensions(available_extension_count, 0);
		VK_CHECK(vkEnumerateInstanceExtensionProperties(0, &available_extension_count, available_extensions.data));

		for (uint32 i = 0; i < extension_count; i++)
		{
			SHMDEBUGV("Searching for extension: %s...", extension_names[i]);
			bool8 found = false;
			for (uint32 j = 0; j < available_extension_count; j++)
			{
				if (CString::equal((char*)extension_names[i], available_extensions[j].extensionName))
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				SHMFATALV("Failed to find required vulkan validation extensions: %s!", extension_names[i]);
				return false;
			}
		}

		SHMDEBUG("All required vulkan validation extension present.");

		inst_create_info.enabledExtensionCount = extension_count;
		inst_create_info.ppEnabledExtensionNames = extension_names;

#if defined(_DEBUG)
		const uint32 validation_layer_count = 1;
		const char* validation_layer_names[validation_layer_count] =
		{
			"VK_LAYER_KHRONOS_validation",
			// "VK_LAYER_LUNARG_api_dump"
		};	

		SHMDEBUG("Vulkan Validation layers enabled.");

		uint32 available_layer_count = 0;
		VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
		Sarray<VkLayerProperties> available_layers(available_layer_count, 0);
		VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.data));

		for (uint32 i = 0; i < validation_layer_count; i++)
		{
			SHMDEBUGV("Searching for layer: %s...", validation_layer_names[i]);
			bool8 found = false;
			for (uint32 j = 0; j < available_layer_count; j++)
			{
				if (CString::equal((char*)validation_layer_names[i], available_layers[j].layerName))
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				SHMFATALV("Failed to find required vulkan validation layers: %s!", validation_layer_names[i]);
				return false;
			}
		}

		SHMDEBUG("All required vulkan validation layers present.");

		inst_create_info.enabledLayerCount = validation_layer_count;
		inst_create_info.ppEnabledLayerNames = validation_layer_names;

#endif

		//NOTE: The second agument for this function is meant to contain callbacks for custom memory allocation.
		VK_CHECK(vkCreateInstance(&inst_create_info, context->allocator_callbacks, &context->instance));

#if defined(_DEBUG)
		SHMDEBUG("Creating Vulkan Debugger...");
		uint32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;

		VkDebugUtilsMessengerCreateInfoEXT debugger_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		debugger_create_info.messageSeverity = log_severity;
		debugger_create_info.messageType = 
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debugger_create_info.pfnUserCallback = vk_debug_callback;

		PFN_vkCreateDebugUtilsMessengerEXT func =
			(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT");
		SHMASSERT_MSG(func, "Failed to create vulkan debug messenger!");
		VK_CHECK(func(context->instance, &debugger_create_info, context->allocator_callbacks, &context->debug_messenger));
		SHMDEBUG("Vulkan debug messenger created.");

		context->debug_set_utils_object_name = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(context->instance, "vkSetDebugUtilsObjectNameEXT");
		if (!context->debug_set_utils_object_name) {
			SHMWARN("Unable to load function pointer for vkSetDebugUtilsObjectNameEXT. Debug functions associated with this will not work.");
		}
		context->debug_set_utils_object_tag = (PFN_vkSetDebugUtilsObjectTagEXT)vkGetInstanceProcAddr(context->instance, "vkSetDebugUtilsObjectTagEXT");
		if (!context->debug_set_utils_object_tag) {
			SHMWARN("Unable to load function pointer for vkSetDebugUtilsObjectTagEXT. Debug functions associated with this will not work.");
		}
		context->debug_begin_utils_label = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(context->instance, "vkCmdBeginDebugUtilsLabelEXT");
		if (!context->debug_begin_utils_label) {
			SHMWARN("Unable to load function pointer for vkCmdBeginDebugUtilsLabelEXT. Debug functions associated with this will not work.");
		}
		context->debug_end_utils_label = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(context->instance, "vkCmdEndDebugUtilsLabelEXT");
		if (!context->debug_end_utils_label) {
			SHMWARN("Unable to load function pointer for vkCmdEndDebugUtilsLabelEXT. Debug functions associated with this will not work.");
		}
#endif

		SHMDEBUG("Creating vulkan surface...");
		if (!create_vulkan_surface())
		{
			SHMERROR("Failed to create vulkan surface");
			return false;
		}
		SHMDEBUG("Vulkan surface created.");

		SHMDEBUG("Creating vulkan device...");
		if (!vk_device_create())
		{
			SHMERROR("Failed to create vulkan device.");
			return false;
		}
		SHMDEBUG("Vulkan device created.");

		vk_swapchain_create(context->framebuffer_width, context->framebuffer_height, &context->swapchain);

		for (uint32 i = 0; i < RendererConfig::framebuffer_count; i++)
			context->framebuffer_fences_in_flight[i] = 0;

		*out_window_render_target_count = context->swapchain.render_textures.capacity;

		create_command_buffers();

		context->image_available_semaphores.init(context->swapchain.max_frames_in_flight, 0, AllocationTag::Renderer);
		context->queue_complete_semaphores.init(context->swapchain.max_frames_in_flight, 0, AllocationTag::Renderer);

		for (uint32 i = 0; i < context->swapchain.max_frames_in_flight; i++)
		{
			VkSemaphoreCreateInfo sem_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			vkCreateSemaphore(context->device.logical_device, &sem_create_info, context->allocator_callbacks, &context->image_available_semaphores[i]);
			vkCreateSemaphore(context->device.logical_device, &sem_create_info, context->allocator_callbacks, &context->queue_complete_semaphores[i]);

			VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			VK_CHECK(vkCreateFence(context->device.logical_device, &fence_create_info, context->allocator_callbacks, &context->framebuffer_fences[i]));
		}

		context->end_of_frame_task_queue.init(100, 0, AllocationTag::Renderer);

		SHMINFO("Vulkan instance initialized successfully!");
		return true;
	}

	void shutdown()
	{		

		vkDeviceWaitIdle(context->device.logical_device);

		SHMDEBUG("Destroying vulkan semaphores and fences...");
		for (uint32 i = 0; i < context->swapchain.max_frames_in_flight; i++)
		{
			if (context->image_available_semaphores[i])
				vkDestroySemaphore(context->device.logical_device, context->image_available_semaphores[i], context->allocator_callbacks);
			context->image_available_semaphores[i] = 0;

			if (context->queue_complete_semaphores[i])
				vkDestroySemaphore(context->device.logical_device, context->queue_complete_semaphores[i], context->allocator_callbacks);
			context->queue_complete_semaphores[i] = 0;

			if (context->framebuffer_fences[i])
				vkDestroyFence(context->device.logical_device, context->framebuffer_fences[i], context->allocator_callbacks);
			context->framebuffer_fences[i] = 0;
		}

		context->image_available_semaphores.free_data();
		context->queue_complete_semaphores.free_data();	

		SHMDEBUG("Destroying vulkan swapchain...");
		vk_swapchain_destroy(&context->swapchain);

		SHMDEBUG("Destroying vulkan device...");
		vk_device_destroy();

		SHMDEBUG("Destroying vulkan surface...");
		if (context->surface)
		{
			vkDestroySurfaceKHR(context->instance, context->surface, context->allocator_callbacks);
			context->surface = 0;
		}	

#if defined(_DEBUG)
		if (context->debug_messenger)
		{
			SHMDEBUG("Destroying vulkan debugger...");
			PFN_vkDestroyDebugUtilsMessengerEXT func =
				(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT");
			func(context->instance, context->debug_messenger, context->allocator_callbacks);
		}
#endif

		SHMDEBUG("Destroying vulkan instance...");
		vkDestroyInstance(context->instance, context->allocator_callbacks);

		if (context->allocator_callbacks)
		{
			Memory::free_memory(context->allocator_callbacks);
			context->allocator_callbacks = 0;
		}
	}

	void vk_device_sleep_till_idle()
	{
		vkDeviceWaitIdle(context->device.logical_device);
	}

	void on_config_changed()
	{
		context->config_changed = true;
	}

	void on_resized(uint32 width, uint32 height)
	{
		context->framebuffer_width = width;
		context->framebuffer_height = height;
		context->framebuffer_size_generation++;
	}

	bool8 vk_begin_frame(const FrameData* frame_data)
	{
		
		OPTICK_EVENT();

		VulkanDevice* device = &context->device;

		if (context->recreating_swapchain)
		{
			VkResult res = vkDeviceWaitIdle(device->logical_device);
			if (!vk_result_is_success(res))
			{
				SHMERRORV("vulkan_begin_frame vkDeviceWaitIdle (1) failed: '%s'", vk_result_string(res, true));
				return false;
			}
			SHMINFO("Recreated swapchain, booting.");
			return false;
		}

		if (context->framebuffer_size_generation != context->framebuffer_size_last_generation || context->config_changed)
		{
			VkResult res = vkDeviceWaitIdle(device->logical_device);
			if (!vk_result_is_success(res))
			{
				SHMERRORV("vulkan_begin_frame vkDeviceWaitIdle (2) failed: '%s'", vk_result_string(res, true));
				return false;
			}

			context->config_changed = false;

			if (!recreate_swapchain())
				return false;

			SHMINFO("Resized, booting.");
			return false;
		}

		VkResult res = vkWaitForFences(context->device.logical_device, 1, &context->framebuffer_fences[context->bound_sync_object_index], true, UINT64_MAX);
		if (!vk_result_is_success(res))
		{
			SHMERRORV("In-flight fence wait failure! Error: %s", vk_result_string(res, true));
			return false;
		}
		
		if (!vk_swapchain_acquire_next_image_index(
			&context->swapchain, UINT64_MAX, context->image_available_semaphores[context->bound_sync_object_index], 0, &context->bound_framebuffer_index))
		{
			SHMERROR("begin_frame - Failed to acquire next image!");
			return false;
		}

		VulkanCommandBuffer* cmd = &context->graphics_command_buffers[context->bound_framebuffer_index];
		vk_command_buffer_reset(cmd);
		vk_command_buffer_begin(cmd, false, false, false);

		context->viewport_rect = { 0.0f, (float32)context->framebuffer_height, (float32)context->framebuffer_width, -(float32)context->framebuffer_height };
		vk_set_viewport(context->viewport_rect);
		context->scissor_rect = { 0, 0, context->framebuffer_width, context->framebuffer_height };
		vk_set_scissor(context->scissor_rect);

		return true;

	}

	bool8 vk_end_frame(const FrameData* frame_data)
	{
				
		VulkanCommandBuffer* cmd = &context->graphics_command_buffers[context->bound_framebuffer_index];
		
		vk_command_buffer_end(cmd);

		if (context->framebuffer_fences_in_flight[context->bound_framebuffer_index])
		{
			VkResult wait_res = vkWaitForFences(context->device.logical_device, 1, &context->framebuffer_fences_in_flight[context->bound_framebuffer_index], true, UINT64_MAX);
			if (!vk_result_is_success(wait_res))
			{
				SHMFATALV("In-flight fence wait failure! Error: %s", vk_result_string(wait_res, true));
			}
		}

		context->framebuffer_fences_in_flight[context->bound_framebuffer_index] = context->framebuffer_fences[context->bound_sync_object_index];

		VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmd->handle;

		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &context->queue_complete_semaphores[context->bound_sync_object_index];
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &context->image_available_semaphores[context->bound_sync_object_index];

		VkPipelineStageFlags flags[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submit_info.pWaitDstStageMask = flags;

		VK_CHECK(vkResetFences(context->device.logical_device, 1, &context->framebuffer_fences[context->bound_sync_object_index]));
		VkResult res = vkQueueSubmit(context->device.graphics_queue, 1, &submit_info, context->framebuffer_fences[context->bound_sync_object_index]);
		if (res != VK_SUCCESS)
		{
			SHMERRORV("vkQueueSubmit failed with result: %s", vk_result_string(res, true));
			return false;
		}

		vk_command_buffer_update_submitted(cmd);

		vk_swapchain_present(
			&context->swapchain,
			context->device.present_queue,
			context->queue_complete_semaphores[context->bound_sync_object_index],
			context->bound_framebuffer_index);

		while (context->end_of_frame_task_queue.count)
			process_task(*context->end_of_frame_task_queue.dequeue());

		return true;

	}

	void vk_set_viewport(Math::Vec4f rect)
	{
		VkViewport vp;
		vp.x = rect.x;
		vp.y = rect.y;
		vp.width = rect.z;
		vp.height = rect.w;
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;

		VulkanCommandBuffer* cmd_buffer = &context->graphics_command_buffers[context->bound_framebuffer_index];
		vkCmdSetViewport(cmd_buffer->handle, 0, 1, &vp);
	}

	void vk_reset_viewport()
	{
		vk_set_viewport(context->viewport_rect);
	}

	void vk_set_scissor(Math::Rect2Di rect)
	{
		VkRect2D scissor;
		scissor.offset.x = rect.pos.x;
		scissor.offset.y = rect.pos.y;
		scissor.extent.width = rect.width;
		scissor.extent.height = rect.height;

		VulkanCommandBuffer* cmd_buffer = &context->graphics_command_buffers[context->bound_framebuffer_index];
		vkCmdSetScissor(cmd_buffer->handle, 0, 1, &scissor);
	}

	void vk_reset_scissor()
	{
		vk_set_scissor(context->scissor_rect);
	}

	bool8 vk_texture_create(Texture* texture)
	{
		VkFormat image_format;
		if (texture->flags & TextureFlags::IsDepth)
			image_format = context->device.depth_format;
		else
			image_format = VK_FORMAT_R8G8B8A8_UNORM;

		texture->internal_data.init(sizeof(VulkanImage), 0, AllocationTag::Texture);
		VulkanImage* image = (VulkanImage*)texture->internal_data.data;

		VkImageUsageFlags usage = 0;
		VkImageAspectFlags aspect = 0;

		if (texture->flags & TextureFlags::IsDepth)
		{
			usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		else
		{
			usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		vk_image_create(
			texture->type,
			texture->width,
			texture->height,
			image_format,
			VK_IMAGE_TILING_OPTIMAL,
			usage,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			true,
			aspect,
			image);

		texture->flags |= TextureFlags::IsLoaded;
		return true;
	}

	static VkFormat channel_count_to_format(uint32 channel_count, VkFormat default_format) {
		switch (channel_count) {
		case 1:
			return VK_FORMAT_R8_UNORM;
		case 2:
			return VK_FORMAT_R8G8_UNORM;
		case 3:
			return VK_FORMAT_R8G8B8_UNORM;
		case 4:
			return VK_FORMAT_R8G8B8A8_UNORM;
		default:
			return default_format;
		}
	}

	void vk_texture_resize(Texture* texture, uint32 width, uint32 height)
	{
		VulkanImage* image = (VulkanImage*)texture->internal_data.data;
		if (image)
		{
			vk_image_destroy(image);

			texture->width = width;
			texture->height = height;

			vk_texture_create(texture);
		}		
	}

	bool8 vk_texture_write_data(Texture* t, uint32 offset, uint32 size, const uint8* pixels)
	{
		VkFormat image_format = channel_count_to_format(t->channel_count, VK_FORMAT_R8G8B8A8_UNORM);
		return vk_image_write_data((VulkanImage*)t->internal_data.data, image_format, t->type, offset, size, pixels);
	}

	bool8 vk_texture_read_data(Texture* t, uint32 offset, uint32 size, void* out_memory)
	{
		VkFormat image_format = channel_count_to_format(t->channel_count, VK_FORMAT_R8G8B8A8_UNORM);
		return vk_image_read_data((VulkanImage*)t->internal_data.data, image_format, t->type, offset, size, out_memory);
	}

	bool8 vk_texture_read_pixel(Texture* t, uint32 x, uint32 y, uint32* out_rgba)
	{
		VkFormat image_format = channel_count_to_format(t->channel_count, VK_FORMAT_R8G8B8A8_UNORM);
		return vk_image_read_pixel((VulkanImage*)t->internal_data.data, image_format, t->type, x, y, out_rgba);
	}

	void vk_texture_destroy(Texture* texture)
	{
		vkDeviceWaitIdle(context->device.logical_device);

		VulkanImage* image = (VulkanImage*)texture->internal_data.data;
		if (image)
			vk_image_destroy(image);			

		texture->internal_data.free_data();
	}

	Texture* vk_get_color_attachment(uint32 index)
	{
		if (index >= context->swapchain.render_textures.capacity) {
			SHMFATALV("Failed to get color attachment index out of range: %u. Attachment count: %u", index, context->swapchain.render_textures.capacity);
			return 0;
		}

		return &context->swapchain.render_textures[index];
	}

	Texture* vk_get_depth_attachment(uint32 attachment_index)
	{
		if (attachment_index >= context->swapchain.depth_textures.capacity) {
			SHMFATALV("Failed to get attachment index out of range: %u. Attachment count: %u", attachment_index, context->swapchain.depth_textures.capacity);
			return 0;
		}

		return &context->swapchain.depth_textures[attachment_index];
	}

	uint32 vk_get_window_attachment_index()
	{
		return context->bound_framebuffer_index;
	}

	uint32 vk_get_window_attachment_count()
	{
		return context->swapchain.render_textures.capacity;
	}

	bool8 vk_is_multithreaded()
	{
		return context->is_multithreaded;
	}

	static int32 find_memory_index(uint32 type_filter, uint32 property_flags)
	{
		VkPhysicalDeviceMemoryProperties memory_properties;
		vkGetPhysicalDeviceMemoryProperties(context->device.physical_device, &memory_properties);

		for (uint32 i = 0; i < memory_properties.memoryTypeCount; i++)
		{
			if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags))
				return i;
		}

		SHMWARN("Unable to find suitable memory type!");
		return -1;
	}

	static void create_command_buffers()
	{
		if (!context->graphics_command_buffers.data)
		{
			context->graphics_command_buffers.init(context->swapchain.render_textures.capacity, 0, AllocationTag::Renderer);
			for (uint32 i = 0; i < context->graphics_command_buffers.capacity; i++)
				context->graphics_command_buffers[i] = {};
		}

		for (uint32 i = 0; i < context->graphics_command_buffers.capacity; i++)
		{
			if (context->graphics_command_buffers[i].handle)
				vk_command_buffer_free(context->device.graphics_command_pool, &context->graphics_command_buffers[i]);
			context->graphics_command_buffers[i] = {};
			vk_command_buffer_allocate(context->device.graphics_command_pool, true, &context->graphics_command_buffers[i]);
		}

		if (context->texture_write_command_buffer.handle)
			vk_command_buffer_free(context->device.graphics_command_pool, &context->texture_write_command_buffer);
		vk_command_buffer_allocate(context->device.graphics_command_pool, true, &context->texture_write_command_buffer);

		SHMDEBUG("Command buffers created.");
	}

	static bool8 recreate_swapchain()
	{

		if (context->recreating_swapchain)
		{
			SHMDEBUG("recreate_swapchain when already recreating swapchain. Booting.");
			return false;
		}

		if (context->framebuffer_width <= 0 || context->framebuffer_height <= 0)
		{
			SHMDEBUG("recreate_swapchain called when framebuffer dimensions are <= 0. Booting.");
			return false;
		}

		context->recreating_swapchain = true;
		vkDeviceWaitIdle(context->device.logical_device);

		for (uint32 i = 0; i < RendererConfig::framebuffer_count; i++)
			context->framebuffer_fences_in_flight[i] = 0;

		vk_device_query_swapchain_support(context->device.physical_device, context->surface, &context->device.swapchain_support);
		vk_device_detect_depth_format(&context->device);

		vk_swapchain_recreate(context->framebuffer_width, context->framebuffer_height, &context->swapchain);

		context->framebuffer_size_last_generation = context->framebuffer_size_generation;

		for (uint32 i = 0; i < context->swapchain.render_textures.capacity; i++)
			vk_command_buffer_free(context->device.graphics_command_pool, &context->graphics_command_buffers[i]);

		Event::event_fire(SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, 0, {});

		create_command_buffers();
		context->recreating_swapchain = false;

		return true;

	}

	static void process_task(TaskInfo task)
	{
		switch (task.type)
		{
		case TaskType::SetImageLayout:
			{
				if (task.set_image_layout.image->handle)
					task.set_image_layout.image->layout = task.set_image_layout.new_layout;

				break;
			}
		}
	}

#if VULKAN_USE_CUSTOM_ALLOCATOR == 1

#define LOG_TRACE_ALLOC 0
#if LOG_TRACE_ALLOC == 1
#define ALLOC_TRACE(x) SHMTRACE(x)
#define ALLOC_ERROR(x) SHMERROR(x)
#define ALLOC_TRACEV(x, ...) SHMTRACEV(x, ##__VA_ARGS__)
#define ALLOC_ERRORV(x, ...) SHMERRORV(x, ##__VA_ARGS__)
#else
#define ALLOC_TRACE(x)
#define ALLOC_ERROR(x)
#define ALLOC_TRACEV(x, ...)
#define ALLOC_ERRORV(x, ...)
#endif

	static void* vkAllocationFunction_callback(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope scope)
	{
		if (!size)
			return 0;

		void* ret = Memory::allocate(size, AllocationTag::Vulkan, (uint16)alignment);
		if (!ret)
		{
			ALLOC_ERROR("VulkanAlloc: Failed to allocate memory block.");
			return 0;
		}

		ALLOC_TRACEV("VulkanAlloc: Allocated block. Size=%lu, alignment=%lu.", size, alignment);
		return ret;
	}

	static void vkFreeFunction_callback(void* user_data, void* memory)
	{
		if (!memory)
			return;

		Memory::free_memory(memory);
		ALLOC_TRACE("VulkanAlloc: Freed block.");
	}

	static void* vkReallocationFunction_callback(void* user_data, void* original, size_t size, size_t alignment, VkSystemAllocationScope scope)
	{
		if (!original)
			return vkAllocationFunction_callback(user_data, size, alignment, scope);

		if (!size)
		{
			vkFreeFunction_callback(user_data, original);
			return 0;
		}

		void* ret = Memory::reallocate(size, original, (uint16)alignment);
		if (!ret)
		{
			ALLOC_ERRORV("VulkanAlloc: Failed to reallocate memory block.", size, alignment);
			return 0;
		}

		ALLOC_TRACEV("VulkanAlloc: Reallocated block. New size=%lu, alignment=%lu.", size, alignment);
		return ret;
		
	}

	static void vkInternalAllocationNotification_callback(void* user_data, size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope)
	{
		ALLOC_TRACEV("VulkanAlloc: External allocation: size=%lu.", size);
		Memory::track_external_allocation(size, AllocationTag::VulkanExt);
	}

	static void vkInternalFreeNotification_callback(void* user_data, size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope)
	{
		ALLOC_TRACEV("VulkanAlloc: External free: size=%lu.", size);
		Memory::track_external_free(size, AllocationTag::VulkanExt);
	}

	static void create_vulkan_allocator(VkAllocationCallbacks*& callbacks)
	{
		callbacks = (VkAllocationCallbacks*)Memory::allocate(sizeof(VkAllocationCallbacks), AllocationTag::Vulkan);
		callbacks->pfnAllocation = vkAllocationFunction_callback;
		callbacks->pfnFree = vkFreeFunction_callback;
		callbacks->pfnReallocation = vkReallocationFunction_callback;
		callbacks->pfnInternalAllocation = vkInternalAllocationNotification_callback;
		callbacks->pfnInternalFree = vkInternalFreeNotification_callback;
		callbacks->pUserData = &context;
	}

#else
	static void create_vulkan_allocator(VkAllocationCallbacks*& callbacks) { callbacks = 0; }
#endif

}

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data
)
{
	switch (message_severity)
	{
	default:
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		SHMERROR(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		SHMWARN(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		SHMINFO(callback_data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		SHMTRACE(callback_data->pMessage);
		break;
	}

	return VK_FALSE;
}
