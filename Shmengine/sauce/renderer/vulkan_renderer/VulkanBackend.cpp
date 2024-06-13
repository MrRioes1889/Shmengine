#include "VulkanBackend.hpp"

#include "VulkanTypes.hpp"
#include "VulkanDevice.hpp"
#include "VulkanPlatform.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanRenderpass.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanUtils.hpp"
#include "VulkanImage.hpp"

#include "shaders/VulkanMaterialShader.hpp"
#include "shaders/VulkanUIShader.hpp"

#include "core/Logging.hpp"
#include "containers/Darray.hpp"
#include "utility/String.hpp"
#include "systems/MaterialSystem.hpp"

#include "core/Application.hpp"

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_types,
	const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
	void* user_data
);

namespace Renderer::Vulkan
{

#if _WIN32
	static const uint32 extension_count = 2 + _DEBUG;
	static const char* extension_names[extension_count] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		"VK_KHR_win32_surface",
#if defined(_DEBUG) 
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
	};

#else
	throw an error
#endif	

#if defined(_DEBUG)
	static const uint32 validation_layer_count = 1;
	static const char* validation_layer_names[validation_layer_count] = 
	{
		"VK_LAYER_KHRONOS_validation"
	};
#else
	static const uint32 validation_layer_count = 0;
	static const char** validation_layer_names = 0;
#endif

	static void create_command_buffers();
	static void regenerate_framebuffers();
	static bool32 recreate_swapchain();
	static int32 find_memory_index(uint32 type_filter, uint32 property_flags);
	static bool32 create_buffers();

	static VulkanContext context = {};
	static uint32 cached_framebuffer_width = 0;
	static uint32 cached_framebuffer_height = 0;

	static void upload_data_range(VkCommandPool pool, VkFence fence, VkQueue queue, VulkanBuffer* buffer, uint64 offset, uint64 size, const void* data)
	{
		VkBufferUsageFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		VulkanBuffer staging;
		vulkan_buffer_create(&context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, flags, true, &staging);

		vulkan_buffer_load_data(&context, &staging, 0, size, 0, data);
		vulkan_buffer_copy_to(&context, pool, fence, queue, staging.handle, 0, buffer->handle, offset, size);
		vulkan_buffer_destroy(&context, &staging);
	}

	static void free_data_range(VulkanBuffer* buffer, uint32 offset, uint32 size)
	{

	}

	bool32 init(const char* application_name)
	{

		context.find_memory_index = find_memory_index;

		//TODO: Replace with own memory allocation callbacks!
		context.allocator_callbacks = 0;

		Application::get_framebuffer_size(&cached_framebuffer_width, &cached_framebuffer_height);
		context.framebuffer_width = (cached_framebuffer_width) ? cached_framebuffer_width : 800;
		context.framebuffer_height = (cached_framebuffer_height) ? cached_framebuffer_height : 600;
		cached_framebuffer_width = 0;
		cached_framebuffer_height = 0;

		VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
		app_info.apiVersion = VK_API_VERSION_1_2;
		app_info.pApplicationName = application_name;
		app_info.applicationVersion = VK_MAKE_VERSION(0,0,1);
		app_info.pEngineName = "Shmengine";
		app_info.engineVersion = VK_MAKE_VERSION(0,0,1);

		VkInstanceCreateInfo inst_create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		inst_create_info.pApplicationInfo = &app_info;

#if defined(_DEBUG)
		SHMDEBUG("Required vulkan extensions:");

		for (uint32 i = 0; i < extension_count; i++)
			SHMDEBUG(extension_names[i]);


		SHMDEBUG("Vulkan Validation layers enabled.");

		uint32 available_layer_count = 0;
		VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
		Sarray<VkLayerProperties> available_layers(available_layer_count, AllocationTag::TRANSIENT);
		VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.data));

		for (uint32 i = 0; i < validation_layer_count; i++)
		{
			SHMDEBUGV("Searching for layer: %s...", validation_layer_names[i]);
			bool32 found = false;
			for (uint32 j = 0; j < available_layer_count; j++)
			{
				if (String::equal((char*)validation_layer_names[i], available_layers[j].layerName))
				{
					found = true;
					SHMDEBUG("Found.");
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
#endif

		inst_create_info.enabledExtensionCount = extension_count;
		inst_create_info.ppEnabledExtensionNames = extension_names;

		inst_create_info.enabledLayerCount = validation_layer_count;
		inst_create_info.ppEnabledLayerNames = validation_layer_names;

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

		vulkan_renderpass_create(
			&context, 
			&context.world_renderpass,
			{0, 0}, 
			{context.framebuffer_width, context.framebuffer_height},
			{0.0f, 0.0f, 0.2f, 1.0f}, 
			1.0f, 
			0,
			(RenderPassClearFlag::COLOR_BUFFER | RenderPassClearFlag::DEPTH_BUFFER | RenderPassClearFlag::STENCIL_BUFFER),
			false,
			true);

		vulkan_renderpass_create(
			&context,
			&context.ui_renderpass,
			{ 0, 0 },
			{ context.framebuffer_width, context.framebuffer_height },
			{ 0.0f, 0.0f, 0.0f, 0.0f },
			1.0f,
			0,
			(RenderPassClearFlag::NONE),
			true,
			false);

		regenerate_framebuffers();

		create_command_buffers();

		context.image_available_semaphores.init(context.swapchain.max_frames_in_flight);
		context.queue_complete_semaphores.init(context.swapchain.max_frames_in_flight);

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

		if (!material_shader_create(&context, &context.material_shader))
		{
			SHMERROR("Failed loading basic builtin object shader");
			return false;
		}

		if (!ui_shader_create(&context, &context.ui_shader))
		{
			SHMERROR("Failed loading basic builtin object shader");
			return false;
		}

		create_buffers();

		for (uint32 i = 0; i < VulkanConfig::max_geometry_count; i++)
		{
			context.geometries[i].id = INVALID_OBJECT_ID;
		}

		SHMINFO("Vulkan instance initialized successfully!");
		return true;
	}

	void shutdown()
	{		

		vkDeviceWaitIdle(context.device.logical_device);

		SHMDEBUG("Destroying vulkan buffers...");
		vulkan_buffer_destroy(&context, &context.object_vertex_buffer);
		vulkan_buffer_destroy(&context, &context.object_index_buffer);

		SHMDEBUG("Destroying vulkan shaders...");
		ui_shader_destroy(&context, &context.ui_shader);
		material_shader_destroy(&context, &context.material_shader);

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

		SHMDEBUG("Destroying vulkan framebuffers...");
		for (uint32 i = 0; i < VulkanConfig::frames_count; i++)
		{
			vkDestroyFramebuffer(context.device.logical_device, context.swapchain.framebuffers[i], context.allocator_callbacks);
			vkDestroyFramebuffer(context.device.logical_device, context.world_framebuffers[i], context.allocator_callbacks);
		}
			

		SHMDEBUG("Destroying vulkan renderpass...");
		vulkan_renderpass_destroy(&context, &context.ui_renderpass);
		vulkan_renderpass_destroy(&context, &context.world_renderpass);

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

		if (context.debug_messenger)
		{
			SHMDEBUG("Destroying vulkan debugger...");
			PFN_vkDestroyDebugUtilsMessengerEXT func =
				(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT");
			func(context.instance, context.debug_messenger, context.allocator_callbacks);
		}

		SHMDEBUG("Destroying vulkan instance...");
		vkDestroyInstance(context.instance, context.allocator_callbacks);
	}

	void on_resized(uint32 width, uint32 height)
	{
		cached_framebuffer_width = width;
		cached_framebuffer_height = height;
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
			return false;

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

		context.world_renderpass.dim.width = context.framebuffer_width;
		context.world_renderpass.dim.height = context.framebuffer_height;

		return true;

	}

	void update_global_world_state(const Math::Mat4& projection, const Math::Mat4& view, Math::Vec3f view_position, Math::Vec4f ambient_colour, int32 mode) 
	{		

		material_shader_use(&context, &context.material_shader);

		context.material_shader.global_ubo.projection = projection;
		context.material_shader.global_ubo.view = view;

		// TODO: other ubo properties

		material_shader_update_global_state(&context, &context.material_shader);

	}

	void update_global_ui_state(const Math::Mat4& projection, const Math::Mat4& view, int32 mode)
	{

		ui_shader_use(&context, &context.ui_shader);

		context.ui_shader.global_ubo.projection = projection;
		context.ui_shader.global_ubo.view = view;

		// TODO: other ubo properties

		ui_shader_update_global_state(&context, &context.ui_shader);

	}

	bool32 end_frame(float32 delta_time)
	{

		VulkanCommandBuffer* cmd = &context.graphics_command_buffers[context.image_index];

		vulkan_command_buffer_end(cmd);

		if (context.images_in_flight[context.image_index])
		{
			VkResult res = vkWaitForFences(context.device.logical_device, 1, context.images_in_flight[context.image_index], true, UINT64_MAX);
			if (!vulkan_result_is_success(res))
			{
				SHMFATALV("In-flight fence wait failure! Error: %s", vulkan_result_string(res, true));
			}
		}

		context.images_in_flight[context.image_index] = &context.fences_in_flight[context.current_frame];
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
			context.device.graphics_queue,
			context.device.present_queue,
			context.queue_complete_semaphores[context.current_frame],
			context.image_index);

		return true;

	}	

	bool32 begin_renderpass(uint32 renderpass_id)
	{

		VulkanRenderpass* renderpass = 0;
		VkFramebuffer framebuffer = 0;
		VulkanCommandBuffer* command_buffer = &context.graphics_command_buffers[context.image_index];

		switch (renderpass_id)
		{
		case (uint32)BuiltinRenderpass::WORLD:
		{
			renderpass = &context.world_renderpass;
			framebuffer = context.world_framebuffers[context.image_index];
			break;
		}
		case (uint32)BuiltinRenderpass::UI:
		{
			renderpass = &context.ui_renderpass;
			framebuffer = context.swapchain.framebuffers[context.image_index];
			break;
		}
		default:
		{
			SHMERRORV("begin_renderpass - called for unrecognized renderpass id: %u", renderpass_id);
			return false;
		}
		}

		vulkan_renderpass_begin(command_buffer, renderpass, framebuffer);

		switch (renderpass_id)
		{
		case (uint32)BuiltinRenderpass::WORLD:
		{
			material_shader_use(&context, &context.material_shader);
			break;
		}
		case (uint32)BuiltinRenderpass::UI:
		{
			ui_shader_use(&context, &context.ui_shader);
			break;
		}
		}

		return true;

	}

	bool32 end_renderpass(uint32 renderpass_id)
	{

		VulkanRenderpass* renderpass = 0;
		VkFramebuffer framebuffer = 0;
		VulkanCommandBuffer* command_buffer = &context.graphics_command_buffers[context.image_index];

		switch (renderpass_id)
		{
		case (uint32)BuiltinRenderpass::WORLD:
		{
			renderpass = &context.world_renderpass;
			framebuffer = context.world_framebuffers[context.image_index];
			break;
		}
		case (uint32)BuiltinRenderpass::UI:
		{
			renderpass = &context.ui_renderpass;
			framebuffer = context.swapchain.framebuffers[context.image_index];
			break;
		}
		default:
		{
			SHMERRORV("end_renderpass - called for unrecognized renderpass id: %u", renderpass_id);
			return false;
		}
		}

		vulkan_renderpass_end(command_buffer, renderpass);
		return true;

	}

	void create_texture(const void* pixels, Texture* texture)
	{

		texture->buffer.init(sizeof(VulkanTextureData), AllocationTag::MAIN);
		VulkanTextureData* data = (VulkanTextureData*)texture->buffer.data;
		VkDeviceSize image_size = texture->width * texture->height * texture->channel_count;

		VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

		VkBufferUsageFlagBits usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		VkMemoryPropertyFlags memory_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		VulkanBuffer staging;
		vulkan_buffer_create(&context, image_size, usage, memory_flags, true, &staging);

		vulkan_buffer_load_data(&context, &staging, 0, image_size, 0, pixels);

		vulkan_image_create(
			&context,
			VK_IMAGE_TYPE_2D,
			texture->width,
			texture->height,
			image_format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			true,
			VK_IMAGE_ASPECT_COLOR_BIT,
			&data->image);

		VulkanCommandBuffer temp_buffer;
		VkCommandPool& pool = context.device.graphics_command_pool;
		VkQueue& queue = context.device.graphics_queue;

		vulkan_command_buffer_reserve_and_begin_single_use(&context, pool, &temp_buffer);
		vulkan_image_transition_layout(&context, &temp_buffer, &data->image, image_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vulkan_image_copy_from_buffer(&context, &data->image, staging.handle, &temp_buffer);		
		vulkan_image_transition_layout(&context, &temp_buffer, &data->image, image_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vulkan_command_buffer_end_single_use(&context, pool, &temp_buffer, queue);

		vulkan_buffer_destroy(&context, &staging);

		VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		// TODO: These filters should be configurable.
		sampler_info.magFilter = VK_FILTER_LINEAR;
		sampler_info.minFilter = VK_FILTER_LINEAR;
		sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_info.anisotropyEnable = VK_TRUE;
		sampler_info.maxAnisotropy = 16;
		sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		sampler_info.unnormalizedCoordinates = VK_FALSE;
		sampler_info.compareEnable = VK_FALSE;
		sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
		sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler_info.mipLodBias = 0.0f;
		sampler_info.minLod = 0.0f;
		sampler_info.maxLod = 0.0f;

		VkResult result = vkCreateSampler(context.device.logical_device, &sampler_info, context.allocator_callbacks, &data->sampler);
		if (!vulkan_result_is_success(VK_SUCCESS)) {
			SHMERRORV("Error creating texture sampler: %s", vulkan_result_string(result, true));
			return;
		}

		texture->generation++;

	}

	void destroy_texture(Texture* texture)
	{

		vkDeviceWaitIdle(context.device.logical_device);

		VulkanTextureData* data = (VulkanTextureData*)texture->buffer.data;
		if (data)
		{
			vulkan_image_destroy(&context, &data->image);
			data->image = {};
			vkDestroySampler(context.device.logical_device, data->sampler, context.allocator_callbacks);
			data->sampler = 0;

			texture->buffer.free_data();
		}
	
		Memory::zero_memory(texture, sizeof(Texture));

	}

	bool32 create_material(Material* material)
	{
		if (!material_shader_acquire_resources(&context, &context.material_shader, material))
		{
			SHMERROR("vulkan_create_material - Failed to acquire shader resources!");
			return false;
		}

		SHMTRACE("Renderer: Material created.");
		return true;
	}

	void destroy_material(Material* material)
	{
		if (material->internal_id != INVALID_OBJECT_ID)
			material_shader_release_resources(&context, &context.material_shader, material);
		else
			SHMWARN("vulkan_destroy_material called when internal material id was already invalid!");
	}

	bool32 create_geometry(Geometry* geometry, uint32 vertex_count, const Vertex3D* vertices, uint32 index_count, const uint32* indices)
	{

		if (!vertices)
		{
			SHMERROR("create_geometry - Supplied vertex and/or index buffer invalid!");
			return false;
		}

		bool32 is_reupload = geometry->id != INVALID_OBJECT_ID;
		VulkanGeometryData old_range = {};

		VulkanGeometryData* internal_data = 0;
		if (is_reupload)
		{
			internal_data = &context.geometries[geometry->internal_id];
			old_range = *internal_data;
		}
		else
		{
			for (uint32 i = 0; i < VulkanConfig::max_geometry_count; i++)
			{
				if (context.geometries[i].id == INVALID_OBJECT_ID)
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

		VkCommandPool& pool = context.device.graphics_command_pool;
		VkQueue& queue = context.device.graphics_queue;

		internal_data->vertex_buffer_offset = (uint32)context.geometry_vertex_offset;
		internal_data->vertex_count = vertex_count;
		internal_data->vertex_size = sizeof(Vertex3D) * vertex_count;
		upload_data_range(pool, 0, queue, &context.object_vertex_buffer, internal_data->vertex_buffer_offset, internal_data->vertex_size, vertices);
		context.geometry_vertex_offset += internal_data->vertex_size;

		if (index_count && indices)
		{
			internal_data->index_buffer_offset = (uint32)context.geometry_index_offset;
			internal_data->index_count = index_count;
			internal_data->index_size = sizeof(Vertex3D) * index_count;
			upload_data_range(pool, 0, queue, &context.object_index_buffer, internal_data->index_buffer_offset, internal_data->index_size, indices);
			context.geometry_index_offset += internal_data->index_size;
		}
		else
		{
			internal_data->index_count = 0;
			internal_data->index_size = 0;
		}

		if (internal_data->generation == INVALID_OBJECT_ID)
			internal_data->generation = 0;
		else
			internal_data->generation++;

		if (is_reupload)
		{
			free_data_range(&context.object_vertex_buffer, old_range.vertex_buffer_offset, old_range.vertex_size);
			if (old_range.index_size)
				free_data_range(&context.object_index_buffer, old_range.index_buffer_offset, old_range.index_size);
		}

		return true;
	}

	void destroy_geometry(Geometry* geometry)
	{
		if (geometry->internal_id != INVALID_OBJECT_ID)
		{
			vkDeviceWaitIdle(context.device.logical_device);

			VulkanGeometryData& internal_data = context.geometries[geometry->internal_id];

			free_data_range(&context.object_vertex_buffer, internal_data.vertex_buffer_offset, internal_data.vertex_size);
			if (internal_data.index_size)
				free_data_range(&context.object_index_buffer, internal_data.index_buffer_offset, internal_data.index_size);

			internal_data = {};
			internal_data.id = INVALID_OBJECT_ID;
			internal_data.generation = INVALID_OBJECT_ID;
		}
	}

	void draw_geometry(const GeometryRenderData& data)
	{

		if (!data.geometry || data.geometry->internal_id == INVALID_OBJECT_ID)
			return;

		VulkanGeometryData& buffer_data = context.geometries[data.geometry->internal_id];
		VulkanCommandBuffer& command_buffer = context.graphics_command_buffers[context.image_index];

		material_shader_use(&context, &context.material_shader);

		material_shader_set_model(&context, &context.material_shader, data.model);

		Material* m = data.geometry->material;
		if (!m)
			m = MaterialSystem::get_default_material();
		material_shader_apply_material(&context, &context.material_shader, m);

		// Bind vertex buffer at offset.
		VkDeviceSize offsets[1] = { buffer_data.vertex_buffer_offset };
		vkCmdBindVertexBuffers(command_buffer.handle, 0, 1, &context.object_vertex_buffer.handle, (VkDeviceSize*)offsets);

		if (buffer_data.index_count)
		{
			// Bind index buffer at offset.
			vkCmdBindIndexBuffer(command_buffer.handle, context.object_index_buffer.handle, buffer_data.index_buffer_offset, VK_INDEX_TYPE_UINT32);

			// Issue the draw.
			vkCmdDrawIndexed(command_buffer.handle, buffer_data.index_count, 1, 0, 0, 0);
		}
		else
		{
			vkCmdDraw(command_buffer.handle, buffer_data.vertex_count, 1, 0, 0);
		}

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
			context.graphics_command_buffers.init(context.swapchain.images.count);
			for (uint32 i = 0; i < context.graphics_command_buffers.count; i++)
				context.graphics_command_buffers[i] = {};
		}

		for (uint32 i = 0; i < context.graphics_command_buffers.count; i++)
		{
			if (context.graphics_command_buffers[i].handle)
				vulkan_command_buffer_free(&context, context.device.graphics_command_pool, &context.graphics_command_buffers[i]);
			context.graphics_command_buffers[i] = {};
			vulkan_command_buffer_allocate(&context, context.device.graphics_command_pool, true, &context.graphics_command_buffers[i]);
		}
			
		SHMDEBUG("Command buffers created.");
	}

	static void regenerate_framebuffers()
	{

		for (uint32 i = 0; i < context.swapchain.images.count; i++)
		{
			const uint32 attachment_count = 2;
			VkImageView attachments[attachment_count] = { context.swapchain.views[i], context.swapchain.depth_attachment.view };

			VkImageView world_attachments[2] = { context.swapchain.views[i], context.swapchain.depth_attachment.view };
			VkFramebufferCreateInfo framebuffer_create_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebuffer_create_info.renderPass = context.world_renderpass.handle;
			framebuffer_create_info.attachmentCount = 2;
			framebuffer_create_info.pAttachments = world_attachments;
			framebuffer_create_info.width = context.framebuffer_width;
			framebuffer_create_info.height = context.framebuffer_height;
			framebuffer_create_info.layers = 1;

			VK_CHECK(vkCreateFramebuffer(context.device.logical_device, &framebuffer_create_info, context.allocator_callbacks, &context.world_framebuffers[i]));

			// Swapchain framebuffers (UI pass). Outputs to swapchain images
			VkImageView ui_attachments[1] = { context.swapchain.views[i] };
			VkFramebufferCreateInfo sc_framebuffer_create_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			sc_framebuffer_create_info.renderPass = context.ui_renderpass.handle;
			sc_framebuffer_create_info.attachmentCount = 1;
			sc_framebuffer_create_info.pAttachments = ui_attachments;
			sc_framebuffer_create_info.width = context.framebuffer_width;
			sc_framebuffer_create_info.height = context.framebuffer_height;
			sc_framebuffer_create_info.layers = 1;

			VK_CHECK(vkCreateFramebuffer(context.device.logical_device, &sc_framebuffer_create_info, context.allocator_callbacks, &context.swapchain.framebuffers[i]));

		}

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

		for (uint32 i = 0; i < context.swapchain.images.count; i++)
			context.images_in_flight[i] = 0;

		vulkan_device_query_swapchain_support(context.device.physical_device, context.surface, &context.device.swapchain_support);
		vulkan_device_detect_depth_format(&context.device);

		vulkan_swapchain_recreate(&context, cached_framebuffer_width, cached_framebuffer_height, &context.swapchain);

		context.framebuffer_width = cached_framebuffer_width;
		context.framebuffer_height = cached_framebuffer_height;	
		context.world_renderpass.dim.width = context.framebuffer_width;
		context.world_renderpass.dim.height = context.framebuffer_height;
		cached_framebuffer_width = 0;
		cached_framebuffer_height = 0;

		context.framebuffer_size_last_generation = context.framebuffer_size_generation;

		for (uint32 i = 0; i < context.swapchain.images.count; i++)
			vulkan_command_buffer_free(&context, context.device.graphics_command_pool, &context.graphics_command_buffers[i]);

		for (uint32 i = 0; i < VulkanConfig::frames_count; i++)
		{
			vkDestroyFramebuffer(context.device.logical_device, context.swapchain.framebuffers[i], context.allocator_callbacks);
			vkDestroyFramebuffer(context.device.logical_device, context.world_framebuffers[i], context.allocator_callbacks);
		}
			

		context.world_renderpass.offset.x = 0;
		context.world_renderpass.offset.y = 0;
		context.world_renderpass.dim.width = context.framebuffer_width;
		context.world_renderpass.dim.height = context.framebuffer_height;

		regenerate_framebuffers();
		create_command_buffers();
		context.recreating_swapchain = false;

		return true;

	}

	static bool32 create_buffers()
	{

		VkMemoryPropertyFlagBits memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		const uint64 vertex_buffer_size = sizeof(Vertex3D) * 1024 * 1024;
		if (!vulkan_buffer_create(
			&context,
			vertex_buffer_size,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
			memory_property_flags,
			true,
			&context.object_vertex_buffer))
		{
			SHMERROR("Error creating vertex buffer;");
			return false;
		}
		context.geometry_vertex_offset = 0;

		const uint64 index_buffer_size = sizeof(uint32) * 1024 * 1024;
		if (!vulkan_buffer_create(
			&context,
			index_buffer_size,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
			memory_property_flags,
			true,
			&context.object_index_buffer))
		{
			SHMERROR("Error creating index buffer;");
			return false;
		}
		context.geometry_index_offset = 0;

		return true;

	}

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

