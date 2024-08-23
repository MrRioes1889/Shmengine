#include "VulkanBackend.hpp"

#include "VulkanTypes.hpp"
#include "VulkanDevice.hpp"
#include "VulkanPlatform.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanUtils.hpp"
#include "VulkanImage.hpp"

#include "core/Logging.hpp"
#include "containers/Darray.hpp"
#include "utility/CString.hpp"

#include "systems/ShaderSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/ResourceSystem.hpp"

// TODO: Get rid of frontend include
#include "renderer/RendererFrontend.hpp"

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
	static bool32 recreate_swapchain();
	static int32 find_memory_index(uint32 type_filter, uint32 property_flags);
	static void create_vulkan_allocator(VkAllocationCallbacks*& callbacks);

	VulkanContext context = {};

	bool32 init(const BackendConfig& config, uint32* out_window_render_target_count)
	{

		context.find_memory_index = find_memory_index;

		context.is_multithreaded = false;

		create_vulkan_allocator(context.allocator_callbacks);

		context.on_render_target_refresh_required = config.on_render_target_refresh_required;

		context.framebuffer_width = 800;
		context.framebuffer_height = 600;

		VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app_info.apiVersion = VK_API_VERSION_1_2;
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
			bool32 found = false;
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
			bool32 found = false;
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
		VK_CHECK(vkCreateInstance(&inst_create_info, context.allocator_callbacks, &context.instance));

#if defined(_DEBUG)
		SHMDEBUG("Creating Vulkan Debugger...");
		uint32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

		VkDebugUtilsMessengerCreateInfoEXT debugger_create_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		debugger_create_info.messageSeverity = log_severity;
		debugger_create_info.messageType = 
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debugger_create_info.pfnUserCallback = vk_debug_callback;

		PFN_vkCreateDebugUtilsMessengerEXT func =
			(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT");
		SHMASSERT_MSG(func, "Failed to create vulkan debug messenger!");
		VK_CHECK(func(context.instance, &debugger_create_info, context.allocator_callbacks, &context.debug_messenger));
		SHMDEBUG("Vulkan debug messenger created.");
#endif

		SHMDEBUG("Creating vulkan surface...");
		if (!Platform::create_vulkan_surface(&context))
		{
			SHMERROR("Failed to create vulkan surface");
			return false;
		}
		SHMDEBUG("Vulkan surface created.");

		SHMDEBUG("Creating vulkan device...");
		if (!vulkan_device_create(&context))
		{
			SHMERROR("Failed to create vulkan device.");
			return false;
		}
		SHMDEBUG("Vulkan device created.");

		vulkan_swapchain_create(&context, context.framebuffer_width, context.framebuffer_height, &context.swapchain);

		*out_window_render_target_count = context.swapchain.render_images.capacity;

		for (uint32 i = 0; i < RendererConfig::renderpass_max_registered; i++)
			context.registered_renderpasses[i].id = INVALID_ID;

		context.renderpass_table.init(RendererConfig::renderpass_max_registered, 0);
		context.renderpass_table.floodfill(INVALID_ID);

		for (uint32 i = 0; i < config.pass_config_count; i++)
		{
			uint32 id = context.renderpass_table.get_value(config.pass_configs[i].name);
			if (id != INVALID_ID)
			{
				SHMERRORV("Hashtable-collision with renderpass named '%s'. Initialization failed.", config.pass_configs[i].name);
				return false;
			}

			for (uint32 j = 0; j < RendererConfig::renderpass_max_registered; j++) {
				if (context.registered_renderpasses[j].id == INVALID_ID) {
					// Found one.
					context.registered_renderpasses[j].id = j;
					id = j;
					break;
				}
			}

			if (id == INVALID_ID)
			{
				SHMERROR("No space was found for a new renderpass. Increase VULKAN_MAX_REGISTERED_RENDERPASSES. Initialization failed.");
				return false;
			}

			context.registered_renderpasses[id].clear_flags = config.pass_configs[i].clear_flags;
			context.registered_renderpasses[id].clear_color = config.pass_configs[i].clear_color;
			context.registered_renderpasses[id].dim = config.pass_configs[i].dim;
			context.registered_renderpasses[id].offset = config.pass_configs[i].offset;

			vk_renderpass_create(&context.registered_renderpasses[id], 1.0f, 0, config.pass_configs[i].prev_name != 0, config.pass_configs[i].next_name != 0);

			context.renderpass_table.set_value(config.pass_configs[i].name, id);
		}

		create_command_buffers();

		context.image_available_semaphores.init(context.swapchain.max_frames_in_flight, 0, AllocationTag::RENDERER);
		context.queue_complete_semaphores.init(context.swapchain.max_frames_in_flight, 0, AllocationTag::RENDERER);

		for (uint32 i = 0; i < context.swapchain.max_frames_in_flight; i++)
		{
			VkSemaphoreCreateInfo sem_create_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			vkCreateSemaphore(context.device.logical_device, &sem_create_info, context.allocator_callbacks, &context.image_available_semaphores[i]);
			vkCreateSemaphore(context.device.logical_device, &sem_create_info, context.allocator_callbacks, &context.queue_complete_semaphores[i]);

			VkFenceCreateInfo fence_create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			VK_CHECK(vkCreateFence(context.device.logical_device, &fence_create_info, context.allocator_callbacks, &context.fences_in_flight[i]));
		}

		for (uint32 i = 0; i < 3; i++)
			context.images_in_flight[i] = 0;

		uint64 vertex_buffer_size = sizeof(Vertex3D) * 1024 * 1024;
		if (!renderbuffer_create(RenderbufferType::VERTEX, vertex_buffer_size, true, &context.object_vertex_buffer))
		{
			SHMERROR("Error creating vertex buffer");
			return false;
		}
		renderbuffer_bind(&context.object_vertex_buffer, 0);

		uint64 index_buffer_size = sizeof(uint32) * 1024 * 1024;
		if (!renderbuffer_create(RenderbufferType::INDEX, index_buffer_size, true, &context.object_index_buffer))
		{
			SHMERROR("Error creating index buffer");
			return false;
		}
		renderbuffer_bind(&context.object_index_buffer, 0);

		for (uint32 i = 0; i < RendererConfig::max_geometry_count; i++)
		{
			context.geometries[i].id = INVALID_ID;
		}

		SHMINFO("Vulkan instance initialized successfully!");
		return true;
	}

	void shutdown()
	{		

		vkDeviceWaitIdle(context.device.logical_device);

		SHMDEBUG("Destroying vulkan buffers...");
		renderbuffer_destroy(&context.object_vertex_buffer);
		renderbuffer_destroy(&context.object_index_buffer);

		SHMDEBUG("Destroying vulkan semaphores and fences...");
		for (uint32 i = 0; i < context.swapchain.max_frames_in_flight; i++)
		{
			if (context.image_available_semaphores[i])
				vkDestroySemaphore(context.device.logical_device, context.image_available_semaphores[i], context.allocator_callbacks);
			context.image_available_semaphores[i] = 0;

			if (context.queue_complete_semaphores[i])
				vkDestroySemaphore(context.device.logical_device, context.queue_complete_semaphores[i], context.allocator_callbacks);
			context.queue_complete_semaphores[i] = 0;

			if (context.fences_in_flight[i])
				vkDestroyFence(context.device.logical_device, context.fences_in_flight[i], context.allocator_callbacks);
			context.fences_in_flight[i] = 0;
		}

		context.image_available_semaphores.free_data();
		context.queue_complete_semaphores.free_data();

		/*SHMDEBUG("Destroying vulkan framebuffers...");
		for (uint32 i = 0; i < VulkanConfig::frames_count; i++)
		{
			render_target_destroy(&context.world_render_targets[i], true);
			render_target_destroy(&context.swapchain.render_targets[i], true);
		}	*/		

		SHMDEBUG("Destroying vulkan renderpass...");
		for (uint32 i = 0; i < RendererConfig::renderpass_max_registered; i++)
		{
			if (context.registered_renderpasses[i].id != INVALID_ID)
				vk_renderpass_destroy(&context.registered_renderpasses[i]);
		}

		SHMDEBUG("Destroying vulkan swapchain...");
		vulkan_swapchain_destroy(&context, &context.swapchain);

		SHMDEBUG("Destroying vulkan device...");
		vulkan_device_destroy(&context);

		SHMDEBUG("Destroying vulkan surface...");
		if (context.surface)
		{
			vkDestroySurfaceKHR(context.instance, context.surface, context.allocator_callbacks);
			context.surface = 0;
		}	

#if defined(_DEBUG)
		if (context.debug_messenger)
		{
			SHMDEBUG("Destroying vulkan debugger...");
			PFN_vkDestroyDebugUtilsMessengerEXT func =
				(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT");
			func(context.instance, context.debug_messenger, context.allocator_callbacks);
		}
#endif

		SHMDEBUG("Destroying vulkan instance...");
		vkDestroyInstance(context.instance, context.allocator_callbacks);

		if (context.allocator_callbacks)
		{
			Memory::free_memory(context.allocator_callbacks);
			context.allocator_callbacks = 0;
		}
	}

	void on_resized(uint32 width, uint32 height)
	{
		context.framebuffer_width = width;
		context.framebuffer_height = height;
		context.framebuffer_size_generation++;

		SHMINFOV("Vulkan renderer backend->resize: w/h/gen: %u/%u/%u", width, height, context.framebuffer_size_generation);
	}

	bool32 begin_frame(float32 delta_time)
	{
		
		context.frame_delta_time = delta_time;
		VulkanDevice* device = &context.device;

		if (context.recreating_swapchain)
		{
			VkResult res = vkDeviceWaitIdle(device->logical_device);
			if (!vulkan_result_is_success(res))
			{
				SHMERRORV("vulkan_begin_frame vkDeviceWaitIdle (1) failed: '%s'", vulkan_result_string(res, true));
				return false;
			}
			SHMINFO("Recreated swapchain, booting.");
			return false;
		}

		if (context.framebuffer_size_generation != context.framebuffer_size_last_generation)
		{
			VkResult res = vkDeviceWaitIdle(device->logical_device);
			if (!vulkan_result_is_success(res))
			{
				SHMERRORV("vulkan_begin_frame vkDeviceWaitIdle (2) failed: '%s'", vulkan_result_string(res, true));
				return false;
			}

			if (!recreate_swapchain())
				return false;

			SHMINFO("Resized, booting.");
			return false;
		}

		VkResult res = vkWaitForFences(context.device.logical_device, 1, &context.fences_in_flight[context.current_frame], true, UINT64_MAX);
		if (!vulkan_result_is_success(res))
		{
			SHMERRORV("In-flight fence wait failure! Error: %s", vulkan_result_string(res, true));
			return false;
		}
		
		if (!vulkan_swapchain_acquire_next_image_index(
			&context, &context.swapchain, UINT64_MAX, context.image_available_semaphores[context.current_frame], 0, &context.image_index))
		{
			SHMERROR("begin_frame - Failed to acquire next image!");
			return false;
		}
			

		VulkanCommandBuffer* cmd = &context.graphics_command_buffers[context.image_index];
		vulkan_command_reset(cmd);
		vulkan_command_buffer_begin(cmd, false, false, false);

		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = (float32)context.framebuffer_height;
		viewport.width = (float32)context.framebuffer_width;
		viewport.height = -(float32)context.framebuffer_height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = context.framebuffer_width;
		scissor.extent.height = context.framebuffer_height;

		vkCmdSetViewport(cmd->handle, 0, 1, &viewport);
		vkCmdSetScissor(cmd->handle, 0, 1, &scissor);

		return true;

	}

	bool32 end_frame(float32 delta_time)
	{

		VulkanCommandBuffer* cmd = &context.graphics_command_buffers[context.image_index];

		vulkan_command_buffer_end(cmd);

		if (context.images_in_flight[context.image_index])
		{
			VkResult res = vkWaitForFences(context.device.logical_device, 1, &context.images_in_flight[context.image_index], true, UINT64_MAX);
			if (!vulkan_result_is_success(res))
			{
				SHMFATALV("In-flight fence wait failure! Error: %s", vulkan_result_string(res, true));
			}
		}

		context.images_in_flight[context.image_index] = context.fences_in_flight[context.current_frame];
		VK_CHECK(vkResetFences(context.device.logical_device, 1, &context.fences_in_flight[context.current_frame]));

		VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmd->handle;

		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &context.queue_complete_semaphores[context.current_frame];
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &context.image_available_semaphores[context.current_frame];

		VkPipelineStageFlags flags[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submit_info.pWaitDstStageMask = flags;

		VkResult res = vkQueueSubmit(context.device.graphics_queue, 1, &submit_info, context.fences_in_flight[context.current_frame]);
		if (res != VK_SUCCESS)
		{
			SHMERRORV("vkQueueSubmit failed width result: %s", vulkan_result_string(res, true));
			return false;
		}

		vulkan_command_update_submitted(cmd);

		vulkan_swapchain_present(
			&context,
			&context.swapchain,
			context.device.present_queue,
			context.queue_complete_semaphores[context.current_frame],
			context.image_index);

		return true;

	}

	static void _texture_create(Texture* texture, VkFormat image_format)
	{
		texture->internal_data.init(sizeof(VulkanImage), 0, AllocationTag::TEXTURE);
		VulkanImage* image = (VulkanImage*)texture->internal_data.data;

		vulkan_image_create(
			&context,
			texture->type,
			texture->width,
			texture->height,
			image_format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			true,
			VK_IMAGE_ASPECT_COLOR_BIT,
			image);

		texture->generation++;
	}

	void vk_texture_create(const void* pixels, Texture* texture)
	{
		
		uint32 image_size = texture->width * texture->height * texture->channel_count * (texture->type == TextureType::TYPE_CUBE ? 6 : 1);
		VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

		_texture_create(texture, image_format);
		vk_texture_write_data(texture, 0, image_size, (uint8*)pixels);	

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

	void vk_texture_create_writable(Texture* texture)
	{

		VkFormat image_format = channel_count_to_format(texture->channel_count, VK_FORMAT_R8G8B8A8_UNORM);
		_texture_create(texture, image_format);

	}

	void vk_texture_resize(Texture* texture, uint32 width, uint32 height)
	{

		VulkanImage* image = (VulkanImage*)texture->internal_data.data;
		if (image)
		{
			vulkan_image_destroy(&context, image);

			VkFormat image_format = channel_count_to_format(texture->channel_count, VK_FORMAT_R8G8B8A8_UNORM);
			texture->width = width;
			texture->height = height;
			_texture_create(texture, image_format);
		}		

	}

	void vk_texture_write_data(Texture* t, uint32 offset, uint32 size, const uint8* pixels)
	{

		VulkanImage* image = (VulkanImage*)t->internal_data.data;

		VkFormat image_format = channel_count_to_format(t->channel_count, VK_FORMAT_R8G8B8A8_UNORM);

		// Create a staging buffer and load data into it.
		Renderbuffer staging;
		if (!renderbuffer_create(RenderbufferType::STAGING, size, false, &staging))
		{
			SHMERROR("vk_texture_write_data - Failed to create staging buffer!");
			return;
		}
		renderbuffer_bind(&staging, 0);

		vk_buffer_load_range(&staging, 0, size, pixels);

		VulkanCommandBuffer temp_buffer;
		VkCommandPool pool = context.device.graphics_command_pool;
		VkQueue queue = context.device.graphics_queue;

		vulkan_command_buffer_reserve_and_begin_single_use(&context, pool, &temp_buffer);
		vulkan_image_transition_layout(&context, t->type, &temp_buffer, image, image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vulkan_image_copy_from_buffer(&context, t->type, image, ((VulkanBuffer*)staging.internal_data.data)->handle, &temp_buffer);
		vulkan_image_transition_layout(&context, t->type, &temp_buffer, image, image_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vulkan_command_buffer_end_single_use(&context, pool, &temp_buffer, queue);

		renderbuffer_unbind(&staging);
		renderbuffer_destroy(&staging);

		t->generation++;

	}

	void vk_texture_destroy(Texture* texture)
	{

		vkDeviceWaitIdle(context.device.logical_device);

		VulkanImage* image = (VulkanImage*)texture->internal_data.data;
		if (image)
		{
			vulkan_image_destroy(&context, image);
			*image = {};

			texture->internal_data.free_data();
		}
	
		Memory::zero_memory(texture, sizeof(Texture));

	}


	bool32 vk_geometry_create(Geometry* geometry, uint32 vertex_size, uint32 vertex_count, const void* vertices, uint32 index_count, const uint32* indices)
	{

		if (!vertices)
		{
			SHMERROR("create_geometry - Supplied vertex and/or index buffer invalid!");
			return false;
		}

		bool32 is_reupload = geometry->internal_id != INVALID_ID;
		VulkanGeometryData old_range = {};

		VulkanGeometryData* internal_data = 0;
		if (is_reupload)
		{
			internal_data = &context.geometries[geometry->internal_id];
			old_range = *internal_data;
		}
		else
		{
			for (uint32 i = 0; i < RendererConfig::max_geometry_count; i++)
			{
				if (context.geometries[i].id == INVALID_ID)
				{
					geometry->internal_id = i;
					context.geometries[i].id = i;
					internal_data = &context.geometries[i];
					break;
				}
			}
		}

		if (!internal_data)
		{
			SHMFATAL("create_geometry - Could not find a free slot for creating vulkan geometry!");
			return false;
		}


		internal_data->vertex_count = vertex_count;
		internal_data->vertex_size = vertex_size;
		uint32 vertices_size = internal_data->vertex_count * internal_data->vertex_size;

		if (!renderbuffer_allocate(&context.object_vertex_buffer, vertices_size, &internal_data->vertex_buffer_offset))
		{
			SHMERROR("vk_geometry_create - Failed to allocate memory from vertex buffer.");
			return false;
		}

		if (!renderbuffer_load_range(&context.object_vertex_buffer, internal_data->vertex_buffer_offset, vertices_size, vertices))
		{
			SHMERROR("vk_geometry_create - Failed to load data into vertex buffer.");
			return false;
		}

		//context.geometry_vertex_offset += vertices_size;

		if (index_count && indices)
		{
			internal_data->index_count = index_count;
			internal_data->index_size = sizeof(uint32);
			uint32 indices_size = internal_data->index_count * internal_data->index_size;

			if (!renderbuffer_allocate(&context.object_index_buffer, indices_size, &internal_data->index_buffer_offset))
			{
				SHMERROR("vk_geometry_create - Failed to allocate memory from index buffer.");
				return false;
			}

			if (!renderbuffer_load_range(&context.object_index_buffer, internal_data->index_buffer_offset, indices_size, indices))
			{
				SHMERROR("vk_geometry_create - Failed to load data into index buffer.");
				return false;
			}
			//context.geometry_index_offset += indices_size;
		}
		else
		{
			internal_data->index_count = 0;
			internal_data->index_size = 0;
		}

		if (internal_data->generation == INVALID_ID)
			internal_data->generation = 0;
		else
			internal_data->generation++;

		if (is_reupload)
		{
			renderbuffer_free(&context.object_vertex_buffer, old_range.vertex_buffer_offset);
			if (old_range.index_size)
				renderbuffer_free(&context.object_index_buffer, old_range.index_buffer_offset);
		}

		return true;
	}

	void vk_geometry_destroy(Geometry* geometry)
	{
		if (geometry->internal_id != INVALID_ID)
		{
			vkDeviceWaitIdle(context.device.logical_device);

			VulkanGeometryData& internal_data = context.geometries[geometry->internal_id];

			renderbuffer_free(&context.object_vertex_buffer, internal_data.vertex_buffer_offset);
			if (internal_data.index_size)
				renderbuffer_free(&context.object_index_buffer, internal_data.index_buffer_offset);

			internal_data = {};
			internal_data.id = INVALID_ID;
			internal_data.generation = INVALID_ID;
		}
	}

	void vk_geometry_draw(const GeometryRenderData& data)
	{

		if (!data.geometry || data.geometry->internal_id == INVALID_ID)
			return;

		VulkanGeometryData& buffer_data = context.geometries[data.geometry->internal_id];
		bool32 includes_indices = buffer_data.index_count > 0;

		vk_buffer_draw(&context.object_vertex_buffer, buffer_data.vertex_buffer_offset, buffer_data.vertex_count, includes_indices);
		if (includes_indices)
			vk_buffer_draw(&context.object_index_buffer, buffer_data.index_buffer_offset, buffer_data.index_count, false);

	}

	Texture* vk_window_attachment_get(uint32 index)
	{
		if (index >= context.swapchain.render_images.capacity) {
			SHMFATALV("Attempting to get attachment index out of range: %u. Attachment count: %u", index, context.swapchain.render_images.capacity);
			return 0;
		}

		return context.swapchain.render_images[index];
	}

	Texture* vk_depth_attachment_get()
	{
		return context.swapchain.depth_texture;
	}

	uint32 vk_window_attachment_index_get()
	{
		return context.image_index;
	}

	bool8 vk_is_multithreaded()
	{
		return context.is_multithreaded;
	}

	static int32 find_memory_index(uint32 type_filter, uint32 property_flags)
	{
		VkPhysicalDeviceMemoryProperties memory_properties;
		vkGetPhysicalDeviceMemoryProperties(context.device.physical_device, &memory_properties);

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
		if (!context.graphics_command_buffers.data)
		{
			context.graphics_command_buffers.init(context.swapchain.render_images.capacity, 0, AllocationTag::RENDERER);
			for (uint32 i = 0; i < context.graphics_command_buffers.capacity; i++)
				context.graphics_command_buffers[i] = {};
		}

		for (uint32 i = 0; i < context.graphics_command_buffers.capacity; i++)
		{
			if (context.graphics_command_buffers[i].handle)
				vulkan_command_buffer_free(&context, context.device.graphics_command_pool, &context.graphics_command_buffers[i]);
			context.graphics_command_buffers[i] = {};
			vulkan_command_buffer_allocate(&context, context.device.graphics_command_pool, true, &context.graphics_command_buffers[i]);
		}
			
		SHMDEBUG("Command buffers created.");
	}

	static bool32 recreate_swapchain()
	{

		if (context.recreating_swapchain)
		{
			SHMDEBUG("recreate_swapchain when already recreating swapchain. Booting.");
			return false;
		}

		if (context.framebuffer_width <= 0 || context.framebuffer_height <= 0)
		{
			SHMDEBUG("recreate_swapchain called when framebuffer dimensions are <= 0. Booting.");
			return false;
		}

		context.recreating_swapchain = true;
		vkDeviceWaitIdle(context.device.logical_device);

		for (uint32 i = 0; i < context.swapchain.render_images.capacity; i++)
			context.images_in_flight[i] = 0;

		vulkan_device_query_swapchain_support(context.device.physical_device, context.surface, &context.device.swapchain_support);
		vulkan_device_detect_depth_format(&context.device);

		vulkan_swapchain_recreate(&context, context.framebuffer_width, context.framebuffer_height, &context.swapchain);

		context.framebuffer_size_last_generation = context.framebuffer_size_generation;

		for (uint32 i = 0; i < context.swapchain.render_images.capacity; i++)
			vulkan_command_buffer_free(&context, context.device.graphics_command_pool, &context.graphics_command_buffers[i]);

		if (context.on_render_target_refresh_required)
			context.on_render_target_refresh_required();

		create_command_buffers();
		context.recreating_swapchain = false;

		return true;

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

		void* ret = Memory::allocate(size, AllocationTag::VULKAN, (uint16)alignment);
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
		Memory::track_external_allocation(size, AllocationTag::VULKAN_EXT);
	}

	static void vkInternalFreeNotification_callback(void* user_data, size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope)
	{
		ALLOC_TRACEV("VulkanAlloc: External free: size=%lu.", size);
		Memory::track_external_free(size, AllocationTag::VULKAN_EXT);
	}

	static void create_vulkan_allocator(VkAllocationCallbacks*& callbacks)
	{
		callbacks = (VkAllocationCallbacks*)Memory::allocate(sizeof(VkAllocationCallbacks), AllocationTag::VULKAN);
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

