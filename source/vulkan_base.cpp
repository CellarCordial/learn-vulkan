#include "vulkan_base.h"
#include "../source/core/tools/log.h"
#include <cstdint>
#include <cstring>
#include <minwindef.h>
#include <set>
#include <vector>
#include "core/math/common.h"
#include "vulkan/vulkan_core.h"
#include "shader/shader_compiler.h"

namespace fantasy
{
	// 调试信息的回调函数.
	VkBool32 debug_call_back(
		VkDebugUtilsMessageSeverityFlagBitsEXT message_serverity,
		VkDebugUtilsMessageTypeFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
		void* user_data
	)
	{
		LOG_ERROR(callback_data->pMessage);
		return VK_FALSE;
	}

	void window_resize_callback(GLFWwindow* window, int width, int height) 
	{
        VulkanBase* app = reinterpret_cast<VulkanBase*>(glfwGetWindowUserPointer(window));
        app->window_resized = true;
    }

	bool VulkanBase::initialize()
	{
		glfwSetWindowUserPointer(_window.get_window(), this);
		glfwSetFramebufferSizeCallback(_window.get_window(), window_resize_callback);

		set_shader_platform(ShaderPlatform::SPIRV);

		ReturnIfFalse(create_instance());

		// 遍历系统支持的扩展, 并全部输出.
		// ReturnIfFalse(enumerate_support_extension());

#ifdef DEBUG
		// 开启调试信息回调, 之开启校验层并不会输出信息.
		ReturnIfFalse(create_debug_utils_messager());
#endif
		ReturnIfFalse(glfwCreateWindowSurface(_instance, _window.get_window(), nullptr, &_surface) == VK_SUCCESS);
		ReturnIfFalse(pick_physical_device());
		ReturnIfFalse(create_device());
		ReturnIfFalse(create_swapchain());
		ReturnIfFalse(create_pipeline());
		ReturnIfFalse(create_frame_buffer());
		ReturnIfFalse(create_command_pool());
		ReturnIfFalse(create_command_buffer());
		ReturnIfFalse(create_sync_objects());
		ReturnIfFalse(create_vertex_buffer());
		return true;
	}


	bool VulkanBase::destroy()
	{
		vkDestroyBuffer(_device, _vertex_buffer, nullptr);
		vkFreeMemory(_device, _vertex_buffer_memory, nullptr);
		vkDestroySemaphore(_device, _back_buffer_avaible_semaphore, nullptr);
		vkDestroySemaphore(_device, _render_finished_semaphore, nullptr);
		vkDestroyFence(_device, _fence, nullptr);

		clean_up_swapchain();
		
		vkFreeCommandBuffers(_device, _cmd_pool, 1, &_cmd_buffer);
		vkDestroyCommandPool(_device, _cmd_pool, nullptr);
		vkDestroyPipeline(_device, _graphics_pipeline, nullptr);
		vkDestroyPipelineLayout(_device, _layout, nullptr);
		vkDestroyRenderPass(_device, _render_pass, nullptr);

#ifdef DEBUG
		ReturnIfFalse(destroy_debug_utils_messager());
#endif
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyDevice(_device, nullptr);
		vkDestroyInstance(_instance, nullptr);
		return true;
	}

	bool VulkanBase::render_loop()
	{
		return draw();
	}

	bool VulkanBase::create_instance()
	{
		// Instance info 需要 layers 和 extensions.
		VkInstanceCreateInfo instance_info{};
		instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Vulkan Test";

		// 和 SPIR-V 版本有关.
		app_info.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
		app_info.engineVersion = VK_MAKE_VERSION(1, 3, 0);
		app_info.apiVersion = VK_API_VERSION_1_3;	
		
		instance_info.pApplicationInfo = &app_info;


		uint32_t glfw_extension_count = 0;
		const CHAR** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
		_instance_extensions.insert(_instance_extensions.end(), glfw_extensions, glfw_extensions + glfw_extension_count);

#ifdef DEBUG
		// LunarG 的 Vulkan SDK 允许我们通过 VKLAYER_KHRONOS_validation 来隐式地开启所有可用的校验层.
		_validation_layers.push_back("VK_LAYER_KHRONOS_validation");

		// 检查系统是否支持 _validation_layers 内的所有校验层, 
		// 现在只有 VK_LAYER_KHRONOS_validation 一个校验层等待检查.
		ReturnIfFalse(check_validation_layer_support());
		
		instance_info.enabledLayerCount = static_cast<uint32_t>(_validation_layers.size());
		instance_info.ppEnabledLayerNames = _validation_layers.data();


		_instance_extensions.push_back("VK_EXT_debug_utils");
#endif

		instance_info.enabledExtensionCount = static_cast<uint32_t>(_instance_extensions.size());
		instance_info.ppEnabledExtensionNames = _instance_extensions.data();

		return vkCreateInstance(&instance_info, nullptr, &_instance) == VK_SUCCESS;
	}

	bool VulkanBase::check_validation_layer_support()
	{
		uint32_t layer_count = 0;
		ReturnIfFalse(vkEnumerateInstanceLayerProperties(&layer_count, nullptr) == VK_SUCCESS);
		std::vector<VkLayerProperties> properties(layer_count);
		ReturnIfFalse(vkEnumerateInstanceLayerProperties(&layer_count, properties.data()) == VK_SUCCESS);

		for (const auto& layer : _validation_layers)
		{
			bool found = false;
			for (const auto& property : properties)
			{
				if (strcmp(layer, property.layerName) == 0)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				LOG_ERROR("Extension " + std::string(layer) + " is not support.");
				return false;
			}
		}
		return true;
	}

	bool VulkanBase::enumerate_support_extension()
	{
		uint32_t extension_count = 0;
		ReturnIfFalse(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr) == VK_SUCCESS);
		std::vector<VkExtensionProperties> extension_properties(extension_count);
		ReturnIfFalse(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extension_properties.data()) == VK_SUCCESS);
		for (uint32_t ix = 0; ix < extension_count; ++ix)
		{
			LOG_INFO(extension_properties[ix].extensionName);
		}
		return true;
	}

	bool VulkanBase::create_debug_utils_messager()
	{
		VkDebugUtilsMessengerCreateInfoEXT debug_util_info{};
		debug_util_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debug_util_info.messageSeverity = 
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debug_util_info.messageType = 
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debug_util_info.pfnUserCallback = debug_call_back;

		// 于 vkCreateDebugUtilsMessengerEXT 函数是一个扩展函数, 不会被 Vulkan 库自动加载, 
		// 所以需要我们自己使用 vkGetInstanceProcAddr 函数来加载它.
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT");
		
		if (func)
		{
			func(_instance, &debug_util_info, nullptr, &_debug_callback);
		}
		else
		{
			LOG_ERROR("Get vkCreateDebugUtilsMessengerEXT process address failed.");
			return false;
		}
		return true;
	}

	bool VulkanBase::destroy_debug_utils_messager()
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func)
		{
			func(_instance, _debug_callback, nullptr);
		}
		else
		{
			LOG_ERROR("Get vkDestroyDebugUtilsMessengerEXT process address failed.");
			return false; 
		}
		return true;
	}

	bool VulkanBase::pick_physical_device()
	{
		uint32_t physical_device_count = 0;
		ReturnIfFalse(vkEnumeratePhysicalDevices(_instance, &physical_device_count, nullptr) == VK_SUCCESS);
		ReturnIfFalse(physical_device_count != 0);
		std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
		ReturnIfFalse(vkEnumeratePhysicalDevices(_instance, &physical_device_count, physical_devices.data()) == VK_SUCCESS);

		for (const auto& device : physical_devices)
		{
			VkPhysicalDeviceProperties properties;
			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceProperties(device, &properties);
			vkGetPhysicalDeviceFeatures(device, &features);

			bool device_type_support = 
				properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || 
				properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
			
			if (
				device_type_support && 
				find_queue_family(device) &&
				check_device_extension(device) && 
				check_swapchain_support(device)
			)
			{
				_physical_device = device;
				vkGetPhysicalDeviceMemoryProperties(_physical_device, &_memory_properties);

				break;
			}
		}

		return true;
	}

	bool VulkanBase::find_queue_family(const auto& physical_device)
	{
		// 需要先确立 queue 的需求再创建 vkdevice.
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
		std::vector<VkQueueFamilyProperties> properties(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, properties.data());

		for (uint32_t ix = 0; ix < properties.size(); ++ix)
		{
			VkBool32 present_support = false;

			// 验证所获取的 surface 能否支持该物理设备的 queue family 进行 present.
			if (vkGetPhysicalDeviceSurfaceSupportKHR(physical_device , ix, _surface, &present_support) == VK_SUCCESS && present_support) 
			{
				_queue_family_index.present_index = ix;
			}
			if (properties[ix].queueCount > 0 && properties[ix].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				_queue_family_index.graphics_index = ix;
			}


			if (
				_queue_family_index.graphics_index != INVALID_SIZE_32 && 
				_queue_family_index.graphics_index != INVALID_SIZE_32
			) 
			{
				break;
			}
		}
		return true;
	}


	bool VulkanBase::create_device()
	{
		VkDeviceCreateInfo device_create_info{};
		device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		std::set<uint32_t> queue_family_indices = { _queue_family_index.graphics_index, _queue_family_index.present_index };
		std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

		FLOAT queue_priority = 1.0f;
		for (uint32_t ix : queue_family_indices)
		{
			auto& create_info = queue_create_infos.emplace_back();
			create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			create_info.queueFamilyIndex = ix;
			create_info.queueCount = 1;
			create_info.pQueuePriorities = &queue_priority;
		}

		device_create_info.pQueueCreateInfos = queue_create_infos.data();
		device_create_info.queueCreateInfoCount = 1;
		
		// 暂时不设立 features.
		VkPhysicalDeviceFeatures device_features{};
		device_create_info.pEnabledFeatures = &device_features;

		device_create_info.enabledExtensionCount = static_cast<uint32_t>(_device_extensions.size());
		device_create_info.ppEnabledExtensionNames = _device_extensions.data();
#if DEBUG
		device_create_info.ppEnabledLayerNames = _validation_layers.data();
		device_create_info.enabledLayerCount = static_cast<uint32_t>(_validation_layers.size());
#endif

		ReturnIfFalse(vkCreateDevice(_physical_device, &device_create_info, nullptr, &_device) == VK_SUCCESS);
		vkGetDeviceQueue(_device, _queue_family_index.graphics_index, 0, &_graphics_queue);
		vkGetDeviceQueue(_device, _queue_family_index.present_index, 0, &_present_queue);
		
		return true;  
	}

	bool VulkanBase::check_device_extension(const auto& physical_device)
	{
		_device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		
		// 检查物理设备是否支持所需扩展, 现在就只有交换链这一个设备扩展.
		uint32_t extension_count = 0;
		ReturnIfFalse(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr) == VK_SUCCESS);
		std::vector<VkExtensionProperties> extension_properties(extension_count);
		ReturnIfFalse(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, extension_properties.data()) == VK_SUCCESS);

		std::set<std::string> unavailble_extensions(_device_extensions.begin(), _device_extensions.end());
		for (const auto& property : extension_properties)
		{
			unavailble_extensions.erase(property.extensionName);
		}

		return unavailble_extensions.empty();
	}

	bool VulkanBase::check_swapchain_support(const auto& physical_device)
	{
		ReturnIfFalse(VK_SUCCESS == vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			physical_device, 
			_surface, &_swapchain_info.surface_capabilities
		));

		uint32_t surface_format_count = 0;
		ReturnIfFalse(VK_SUCCESS == vkGetPhysicalDeviceSurfaceFormatsKHR(
			physical_device, 
			_surface, 
			&surface_format_count, 
			nullptr
		));
		ReturnIfFalse(surface_format_count > 0);

		_swapchain_info.surface_formats.resize(surface_format_count);
		ReturnIfFalse(VK_SUCCESS == vkGetPhysicalDeviceSurfaceFormatsKHR(
			physical_device, 
			_surface, 
			&surface_format_count, 
			_swapchain_info.surface_formats.data()
		));

		uint32_t present_mode_count = 0;
		ReturnIfFalse(VK_SUCCESS == vkGetPhysicalDeviceSurfacePresentModesKHR(
			physical_device, 
			_surface, 
			&present_mode_count, 
			nullptr
		));
		ReturnIfFalse(present_mode_count > 0);

		_swapchain_info.present_modes.resize(present_mode_count);
		ReturnIfFalse(VK_SUCCESS == vkGetPhysicalDeviceSurfacePresentModesKHR(
			physical_device, 
			_surface, 
			&present_mode_count, 
			_swapchain_info.present_modes.data()
		));
		
		return true;
	}


	bool VulkanBase::create_swapchain()
	{
		// 确定缓冲区个数.
		// 若 maxImageCount 的值为 0 表明，只要内存可以满足，我们可以使用任意数量的图像.
		uint32_t frames_in_flight_count = std::min(_swapchain_info.surface_capabilities.minImageCount + 1, NUM_FRAMES_IN_FLIGHT);
		
		if (
			_swapchain_info.surface_capabilities.maxImageCount > 0 &&
			frames_in_flight_count > _swapchain_info.surface_capabilities.maxImageCount
		)
		{
			frames_in_flight_count = _swapchain_info.surface_capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR swapchain_create_info{};
		swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_create_info.surface = _surface;
		swapchain_create_info.minImageCount = frames_in_flight_count;

		// 确定缓冲区 format.
		if (
			_swapchain_info.surface_formats.size() == 1 &&
			_swapchain_info.surface_formats[0].format == VK_FORMAT_UNDEFINED
		)
		{
			swapchain_create_info.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
			swapchain_create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		}
		else 
		{
			bool found = false;
			for (const auto& surface_format : _swapchain_info.surface_formats)
			{
				if (
					surface_format.format == VK_FORMAT_R8G8B8A8_UNORM &&
					surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
				)
				{
					found = true;
					swapchain_create_info.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
					swapchain_create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
				}
			}
			if (!found)
			{
				swapchain_create_info.imageFormat = _swapchain_info.surface_formats[0].format;
				swapchain_create_info.imageColorSpace = _swapchain_info.surface_formats[0].colorSpace; 
			}
		}
		_swapchain_format = swapchain_create_info.imageFormat;

		for (const auto& present_mode : _swapchain_info.present_modes)
		{
			if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) { swapchain_create_info.presentMode = present_mode; break; }
			else if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) { swapchain_create_info.presentMode = present_mode; break; }
			else if (present_mode == VK_PRESENT_MODE_FIFO_KHR) { swapchain_create_info.presentMode = present_mode; break; }
		}

		// 确定缓冲区分辨率.
		ReturnIfFalse(update_client_resolution());
		swapchain_create_info.imageExtent = _client_resolution;

		// imageArrayLayers 成员变量用于指定每个图像所包含的层次. 
		// 通常, 来说它的值为 1. 但对于 VR 相关的应用程序来说, 会使用更多的层次.
		swapchain_create_info.imageArrayLayers = 1;

		// imageUsage 成员变量用于指定我们将在图像上进行怎样的操作.
		// 这里只进行绘制操作, 也就是将图像作为一个 color attachment 来使用.
		swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		// clipped 成员变量被设置为 VK_TRUE 表示我们不关心被窗口系统中的其它窗口遮挡的像素的颜色.
		swapchain_create_info.clipped = VK_TRUE;

		uint32_t queue_family_indices[] = { _queue_family_index.graphics_index, _queue_family_index.present_index };
		if (_queue_family_index.graphics_index != _queue_family_index.present_index)
		{
			// 图像可以在多个队列族间使用, 不需要显式地改变图像所有权.
			swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchain_create_info.queueFamilyIndexCount = 2;
			swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
		}
		else 
		{
			// 一张图像同一时间只能被一个队列族所拥有, 在另一队列族使用它之前, 必须显式地改变图像所有权.
			swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchain_create_info.queueFamilyIndexCount = 0;
			swapchain_create_info.pQueueFamilyIndices = nullptr;
		}

		// 我们可以为交换链中的图像指定一个固定的变换操作 (需要交换链具有 supportedTransforms 特性),
		// 比如顺时针旋转 90 度或是水平翻转. 
		// 如果读者不需要进行任何变换操作, 指定使用 currentTransform 变换即可.
		swapchain_create_info.preTransform = _swapchain_info.surface_capabilities.currentTransform;

		// compositeAlpha 成员变量用于指定alpha通道是否被用来和窗口系统中的其它窗口进行混合操作.
		// 设置为 VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR 来忽略掉 alpha 通道.
		swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

		ReturnIfFalse(vkCreateSwapchainKHR(_device, &swapchain_create_info, nullptr, &_swapchain) == VK_SUCCESS);

		// 获取缓冲区 buffer.
		uint32_t back_buffer_count = 0;
		ReturnIfFalse(vkGetSwapchainImagesKHR(_device, _swapchain, &back_buffer_count, nullptr) == VK_SUCCESS);
		_back_buffers.resize(back_buffer_count);
		ReturnIfFalse(vkGetSwapchainImagesKHR(_device, _swapchain, &back_buffer_count, _back_buffers.data()) == VK_SUCCESS);
		
		_back_buffer_views.resize(back_buffer_count);
		for (uint32_t ix = 0; ix < back_buffer_count; ++ix)
		{
			VkImageViewCreateInfo view_create_info{};
			view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view_create_info.image = _back_buffers[ix];
			view_create_info.format = swapchain_create_info.imageFormat;
			
			// components 成员变量用于进行图像颜色通道的映射.
			// 比如, 对于单色纹理, 我们可以将所有颜色通道映射到红色通道. 我们也可以直接将颜色通道的值映射为常数 0 或 1.
			// 这里使用默认的 VK_COMPONENT_SWIZZLE_IDENTITY.
			view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			// subresourceRange 成员变量用于指定图像的用途和图像的哪一部分可以被访问.
			// 这里图像被用作渲染目标, 并且没有细分级别, 只存在一个图层.
			// VK_IMAGE_ASPECT_COLOR_BIT: 表示图像的颜色方面，用于颜色附件或纹理.
			// VK_IMAGE_ASPECT_DEPTH_BIT: 表示图像的深度方面，用于深度缓冲区.
			// VK_IMAGE_ASPECT_STENCIL_BIT: 表示图像的模板方面，用于模板缓冲区.
			view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view_create_info.subresourceRange.baseMipLevel = 0;
			view_create_info.subresourceRange.levelCount = 1;
			view_create_info.subresourceRange.baseArrayLayer = 0;
			view_create_info.subresourceRange.layerCount = 1;

			ReturnIfFalse(vkCreateImageView(_device, &view_create_info, nullptr, &_back_buffer_views[ix]) == VK_SUCCESS);
		}
		return true;
	}

	bool VulkanBase::create_pipeline()
	{
		ShaderCompileDesc vs_desc;
		vs_desc.shader_name = "triangle_vs.slang";
		vs_desc.entry_point = "main";
		vs_desc.target = ShaderTarget::Vertex;
		ShaderData vs_data = compile_shader(vs_desc);
		
		ShaderCompileDesc ps_desc;
		ps_desc.shader_name = "triangle_ps.slang";
		ps_desc.entry_point = "main";
		ps_desc.target = ShaderTarget::Pixel;
		ShaderData ps_data = compile_shader(ps_desc);


		VkShaderModule vs;
		VkShaderModule ps;

		VkShaderModuleCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.codeSize = vs_data.size();
		create_info.pCode = reinterpret_cast<uint32_t*>(vs_data.data());
		ReturnIfFalse(vkCreateShaderModule(_device, &create_info, nullptr, &vs) == VK_SUCCESS);
		create_info.codeSize = ps_data.size();
		create_info.pCode = reinterpret_cast<uint32_t*>(ps_data.data());
		ReturnIfFalse(vkCreateShaderModule(_device, &create_info, nullptr, &ps) == VK_SUCCESS);

		VkPipelineShaderStageCreateInfo vs_stage_create_info{};
		vs_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vs_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vs_stage_create_info.module = vs;
		vs_stage_create_info.pName = vs_desc.entry_point.c_str();

		VkPipelineShaderStageCreateInfo ps_stage_create_info{};
		ps_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		ps_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		ps_stage_create_info.module = ps;
		ps_stage_create_info.pName = ps_desc.entry_point.c_str();
		// 该成员变量指定预编译宏.
		// shader_stage_create_info.pSpecializationInfo;

		VkPipelineShaderStageCreateInfo shader_stage_infos[2] = {
			vs_stage_create_info, ps_stage_create_info
		};

		auto vertex_input_binding = Vertex::get_input_binding_description();
		auto vertex_input_attribute = Vertex::get_input_attribute_description();

		VkPipelineVertexInputStateCreateInfo vertex_input_create_info{};
		vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_create_info.vertexBindingDescriptionCount = 1;
		vertex_input_create_info.pVertexBindingDescriptions = &vertex_input_binding;
		vertex_input_create_info.vertexAttributeDescriptionCount = vertex_input_attribute.size();
		vertex_input_create_info.pVertexAttributeDescriptions = vertex_input_attribute.data();

		VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
		input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		
		// 设置为VK TRUE, 那么使用带有 _STRIP 结尾的图元类型, 
		// 可以通过一个特殊索引值 0xFFFF 或 0xFFFFFFFF 达到重启图元的目的
		// (从特殊索引值之后的索引重置为图元的第一个顶点).
		input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

		_viewport.x = 0.0f;
		_viewport.y = 0.0f;
		_viewport.width = _client_resolution.width;
		_viewport.height = _client_resolution.height;
		_viewport.minDepth = 0.0f;
		_viewport.maxDepth = 1.0f;

		_scissor.offset = { 0, 0 };
		_scissor.extent = { _client_resolution.width, _client_resolution.height };

		// 使用多个视口和裁剪矩形需要启用特性 VkPhysicalDeviceFeatures.multiViewport.
		VkPipelineViewportStateCreateInfo viewport_create_info{};
		viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_create_info.viewportCount = 1;
		viewport_create_info.pViewports = &_viewport;
		viewport_create_info.scissorCount = 1;
		viewport_create_info.pScissors = &_scissor;

		VkPipelineRasterizationStateCreateInfo raster_create_info{};
		raster_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

		// 设置为 VK_TRUE 表示在近平面和远平面外的片段会被截断为在近平面和远平面上, 而不是直接丢弃这些片段.
		// 需启用特性 VkPhysicalDeviceFeatures.depthClamp.
		raster_create_info.depthBiasClamp = VK_FALSE;

		// 设置为 VK_TRUE 表示所有几何图元都不能通过光栅化阶段, 这一设置会禁止一切片段输出到帧缓冲.
		raster_create_info.rasterizerDiscardEnable = VK_FALSE;

		// VK_POLYGON_MODE_FILL：整个多边形，包括多边形内部都产生片段.
		// VK_POLYGON_MODE_LINE：只有多边形的边会产生片段.
		// VK_POLYGON_MODE_POINT：只有多边形的顶点会产生片段.
		raster_create_info.polygonMode = VK_POLYGON_MODE_FILL;

		// lineWidth 成员变量用于指定光栅化后的线段宽度, 它以线宽所占的片段数目为单位.
		// 线宽的最大值依赖于硬件, 使用大于 1.0f 的线宽, 需要启用特性 VkPhysicalDeviceFeatures.wideLines.
		raster_create_info.lineWidth = 1.0f;

		raster_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
		raster_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		
		// 设置为 VK_TRUE 时, 光栅化程序可以添加一个常量值或是一个基于片段所处线段的斜率得到的变量值到深度值上.
		raster_create_info.depthBiasEnable = VK_FALSE;
		raster_create_info.depthBiasConstantFactor = 0.0f;
		raster_create_info.depthBiasClamp = 0.0f;
		raster_create_info.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisample_create_info{};
		multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_create_info.sampleShadingEnable = VK_FALSE;
		multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample_create_info.minSampleShading = 1.0f;
		multisample_create_info.pSampleMask = nullptr;
		multisample_create_info.alphaToCoverageEnable = VK_FALSE;
		multisample_create_info.alphaToOneEnable = VK_FALSE;

		// 深度模板测试.
		// VkPipelineDepthStencilStateCreateInfo

		VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blend_create_info{};
        color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_create_info.logicOpEnable = VK_FALSE;
        color_blend_create_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_create_info.attachmentCount = 1;
        color_blend_create_info.pAttachments = &color_blend_attachment;

		// 包含 4 个浮点数的数组, 表示 RGBA 颜色值. 它的作用是为某些混合因子 (Blend Factors) 提供常量值. 
		// 当 VkPipelineColorBlendAttachmentState.colorBlendOp 或 alphaBlendOP 中使用了以下混合因子
		// VK_BLEND_FACTOR_CONSTANT_COLOR
		// VK_BLEND_FACTOR_CONSTANT_ALPHA
		// VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR
		// VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA
		// 时, blendConstants 的值会被用作输入.
		// blendConstants 的默认值为 [0.0f, 0.0f, 0.0f, 0.0f]
        color_blend_create_info.blendConstants[0] = 0.0f;
        color_blend_create_info.blendConstants[1] = 0.0f;
        color_blend_create_info.blendConstants[2] = 0.0f;
        color_blend_create_info.blendConstants[3] = 0.0f;

		// 只有非常有限的管线状态可以在不重建管线的情况下进行动态修改.
		// 这包括视口大小, 线宽和混合常量 (VkPipelineColorBlendStateCreateInfo.blendConstants).
		VkDynamicState dynamic_state[2] = {
			VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
		};
		// 这样设置后会导致我们之前对这里使用的动态状态的设置被忽略掉, 需要我们在进行绘制时重新指定它们的值.
		VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
		dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_create_info.dynamicStateCount = 2;
		dynamic_state_create_info.pDynamicStates = dynamic_state;


		VkPipelineLayoutCreateInfo layout_create_info{};
		layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		
		// 暂时什么都不需要.
		layout_create_info.setLayoutCount = 0;
		layout_create_info.pSetLayouts = nullptr;
		layout_create_info.pushConstantRangeCount = 0;
		layout_create_info.pPushConstantRanges = nullptr;

		ReturnIfFalse(vkCreatePipelineLayout(_device, &layout_create_info, nullptr, &_layout) == VK_SUCCESS);

		ReturnIfFalse(create_render_pass());

		VkGraphicsPipelineCreateInfo pipeline_create_info{};
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = shader_stage_infos;
		pipeline_create_info.pVertexInputState = &vertex_input_create_info;
		pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
		pipeline_create_info.pViewportState = &viewport_create_info;
		pipeline_create_info.pRasterizationState = &raster_create_info;
		pipeline_create_info.pMultisampleState = &multisample_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_create_info;
		pipeline_create_info.pDynamicState = &dynamic_state_create_info;
		pipeline_create_info.layout = _layout;
		pipeline_create_info.renderPass = _render_pass;
		
		// 若是要应用其他 subpass, 需要再创建一个 VkGraphicsPipeline.
		pipeline_create_info.subpass = 0; 

		// basePipelineHandle 和 basePipelineIndex 成员变量用于以一个创建好的图形管线为基础创建一个新的图形管线.
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

		// 该函数被设计成一次调用可以通过多个 VkGraphicsPipelineCreateInfo 结构体数据创建多个 VkPipeline 对象.
		ReturnIfFalse(VK_SUCCESS == vkCreateGraphicsPipelines(
			_device, 
			VK_NULL_HANDLE, 
			1, 
			&pipeline_create_info, 
			nullptr, 
			&_graphics_pipeline
		));
		
		vkDestroyShaderModule(_device, vs, nullptr);
		vkDestroyShaderModule(_device, ps, nullptr);

		return true;
	}

	bool VulkanBase::create_render_pass()
	{
		VkAttachmentDescription attachment_description{};
		attachment_description.format = _swapchain_format;

		// 没有使用多重采样，所以将采样数设置为 1.
		attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;

		// loadOp 和 storeOp 成员变量用于指定在渲染之前和渲染之后对附着中的数据进行的操作.
		// VK_ATTACHMENT_LOAD_OP_LOAD：保持附着的现有内容.
		// VK_ATTACHMENT_LOAD_OP_CLEAR：使用一个常量值来清除附着的内容.
		// VK_ATTACHMENT_LOAD_OP_DONT_CARE：不关心附着现存的内容.
		// VK_ATTACHMENT_STORE_OP_STORE：渲染的内容会被存储起来, 以便之后读取.
		// VK_ATTACHMENT_STORE_OP_DONT_CARE：渲染后，不会读取帧缓冲的内容.
		attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		// image layout 类似于 resource state.
		// initialLayout 成员变量用于指定渲染流程开始前的图像布局方式. 
		// finalLayout 成员变量用于指定渲染流程结束后的图像布局方式. 
		// 将 initialLayout 成员变量设置为 VK_IMAGE_LAYOUT_UNDEFINED 表示我们不关心之前的图像布局方式.
		// 使用这一值后, 图像的内容不保证会被保留, 但这里，每次渲染前都要清除图像, 所以这样的设置更符合需求. 
		// 对于 finalLayout 成员变量, 设置为 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR，使得渲染后的图像可以被交换链呈现.
		// UNDEFINED 也就是 common, PRESENT_SRC_KHR 也就是 present.
		attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference attachment_ref{};
		attachment_ref.attachment = 0;	// 索引.
		// layout 成员变量用于指定进行子流程时引用的附着使用的布局方式, 
		// Vulkan 会在子流程开始时自动将引用的附着转换到layout成员变量指定的图像布局.
		// COLOR_ATTACHMENT_OPTIMAL 也就是 render target.
		attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};

		// pipelineBindPoint 用于指定子流程绑定的管线类型. 它决定了子流程中使用的管线是图形管线还是计算管线.
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &attachment_ref;
		// pInputAttachments: 被着色器读取的附着.
		// pResolveAttachments: 用于多重采样的颜色附着.
		// pDepthStencilAttachment: 用于深度和模板数据的附着.
		// pPreserveAttachments: 没有被这一子流程使用，但需要保留数据的附着.

		VkSubpassDependency subpass_dependency{};
		subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpass_dependency.dstSubpass = 0;

		// srcStageMask 和 srcAccessMask 成员变量用于指定需要等待的管线阶段和子流程将进行的操作类型.
		// 我们需要等待交换链结束对图像的读取才能对图像进行访问操作, 也就是等待颜色附着输出这一管线阶段.
		subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass_dependency.srcAccessMask = 0;

		// dstStageMask 和 dstAccessMask 成员变量用于指定需要等待的管线阶段和子流程将进行的操作类型.
		// 在这里, 我们的设置为等待颜色附着的输出阶段, 子流程将会进行颜色附着的读写操作.
		// 这样设置后，图像布局变换 (也就是 resource state barrier)直到开始写入颜色数据时时才会进行.
		subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


		// 需要把各个 VkAttachmentDescription 都放进来, 然后 VkSubpassDescription 会根据其 colorAttachmentCount 索引
		// 使用这些 VkAttachmentDescription, 所有 VkSubpassDescription 也放进来, VkSubpassDependency 则会索引他们
		// 根据依赖排列 pass 的执行顺序.

		VkRenderPassCreateInfo render_pass_create_info{};
		render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_create_info.attachmentCount = 1;
		render_pass_create_info.pAttachments = &attachment_description;
		render_pass_create_info.subpassCount = 1;
		render_pass_create_info.pSubpasses = &subpass;

		// VkSubpassDependency 用于定义渲染流程中不同子流程（Subpass）之间的依赖关系
		render_pass_create_info.dependencyCount = 1;
		render_pass_create_info.pDependencies = &subpass_dependency;

		return vkCreateRenderPass(_device, &render_pass_create_info, nullptr, &_render_pass) == VK_SUCCESS;
	}

	bool VulkanBase::create_frame_buffer()
	{
		_frame_buffers.resize(_back_buffers.size());
		for (uint32_t ix = 0; ix < _back_buffers.size(); ++ix)
		{
			VkImageView attachments[] = { _back_buffer_views[ix] };

			VkFramebufferCreateInfo framebuffer_create_info{};
			framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebuffer_create_info.attachmentCount = 1;
			framebuffer_create_info.pAttachments = attachments;
			framebuffer_create_info.renderPass = _render_pass;
			framebuffer_create_info.width = _client_resolution.width;
			framebuffer_create_info.height = _client_resolution.height;
			
			//layers 字段表示帧缓冲的层数.
			// 对于普通的 2D 渲染，layers 设置为 1.
			// 对于多视图渲染 (如 VR 或立方体贴图), layers 可以设置为大于 1 的值.
			framebuffer_create_info.layers = 1;

			ReturnIfFalse(vkCreateFramebuffer(_device, &framebuffer_create_info, nullptr, &_frame_buffers[ix]) == VK_SUCCESS);
		}
		return true;
	}

	bool VulkanBase::create_command_pool()
	{
		VkCommandPoolCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		create_info.queueFamilyIndex = _queue_family_index.graphics_index;
		create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: 使用它分配的
		// 指令缓冲对象被频繁用来记录新的指令 (使用这一标记可能会改变帧缓冲对象的内存分配策略).
		// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: 
		// 指令缓冲对象之间相互独立, 不会被一起重置, 不使用这一标记, 指令缓冲对象会被放在一起重置.

		return vkCreateCommandPool(_device, &create_info, nullptr, &_cmd_pool) == VK_SUCCESS;
	}

	bool VulkanBase::create_command_buffer()
	{
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = _cmd_pool;

		// VK_COMMAND_BUFFER_LEVEL_PRIMARY: 可以被提交到队列进行执行, 但不能被其它指令缓冲对象调用.
		// VK_COMMAND_BUFFER_LEVEL_SECONDARY: 不能直接被提交到队列进行执行, 但可以被主要指令缓冲对象调用执行.
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		ReturnIfFalse(vkAllocateCommandBuffers(_device, &alloc_info, &_cmd_buffer) == VK_SUCCESS);

		return true;
	}

	bool VulkanBase::record_command(uint32_t cmd_buffer_index)
	{
		VkCommandBufferBeginInfo cmd_buffer_begin{};
		cmd_buffer_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: 指令缓冲在执行一次后, 就被用来记录新的指令.
		// VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: 这是一个只在一个渲染流程内使用的辅助指令缓冲.
		// VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: 在指令缓冲等待执行时, 仍然可以提交这一指令缓冲.

		ReturnIfFalse(vkBeginCommandBuffer(_cmd_buffer, &cmd_buffer_begin) == VK_SUCCESS);

		VkRenderPassBeginInfo render_pass_begin_info{};
		render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin_info.renderPass = _render_pass;
		render_pass_begin_info.framebuffer = _frame_buffers[cmd_buffer_index];
		render_pass_begin_info.renderArea.offset = { 0, 0 };
		render_pass_begin_info.renderArea.extent = _client_resolution;

		// 多个 render target 就指定多个 clear value.
		VkClearValue clear_value = { 0.0f, 0.0f, 0.0f, 1.0f };
		render_pass_begin_info.clearValueCount = 1;
		render_pass_begin_info.pClearValues = &clear_value;

		// VK_SUBPASS_CONTENTS_INLINE: 所有要执行的指令都在主要指令缓冲中, 没有辅助指令缓冲需要执行.
		// VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: 有来自辅助指令缓冲的指令需要执行.
		vkCmdBeginRenderPass(_cmd_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline);
		vkCmdSetViewport(_cmd_buffer, 0, 1, &_viewport);
		vkCmdSetScissor(_cmd_buffer, 0, 1, &_scissor);

		VkBuffer vertex_buffers[] = { _vertex_buffer };
		VkDeviceSize vertex_buffer_offsets[] = { 0 };
		vkCmdBindVertexBuffers(_cmd_buffer, 0, 1, vertex_buffers, vertex_buffer_offsets);

		vkCmdDraw(_cmd_buffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(_cmd_buffer);


		return vkEndCommandBuffer(_cmd_buffer) == VK_SUCCESS;
	}

	bool VulkanBase::create_sync_objects()
	{
		VkSemaphoreCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fence_create_info{};
		fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

		//默认情况下, fence 在创建后是未发出信号的状态, 这就意味着如果没有在 vkWaitForFences 函数调用之前
		// 发出 fence 信号, vkWaitForFences 函数调用将会一直处于等待状态, 所以设置其初始状态为已发出信号.
		fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		
		return 	vkCreateSemaphore(_device, &create_info, nullptr, &_back_buffer_avaible_semaphore) == VK_SUCCESS &&
				vkCreateSemaphore(_device, &create_info, nullptr, &_render_finished_semaphore) == VK_SUCCESS &&
				vkCreateFence(_device, &fence_create_info, nullptr, &_fence) == VK_SUCCESS;
	}


	bool VulkanBase::draw()
	{
		ReturnIfFalse(vkWaitForFences(_device, 1, &_fence, VK_TRUE, INVALID_SIZE_64) == VK_SUCCESS);

		uint32_t back_buffer_index = 0;
		VkResult result = vkAcquireNextImageKHR(
			_device, 
			_swapchain, 
			INVALID_SIZE_64, 
			_back_buffer_avaible_semaphore, 
			VK_NULL_HANDLE, 
			&back_buffer_index
		);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			ReturnIfFalse(recreate_swapchain());
			return true;
		}
		else if (result != VK_SUCCESS)
		{
			LOG_ERROR("vkAcquireNextImageKHR() called failed.");
			return false;
		}

		vkResetFences(_device, 1, &_fence); 

		ReturnIfFalse(vkResetCommandBuffer(_cmd_buffer, 0) == VK_SUCCESS);
        ReturnIfFalse(record_command(back_buffer_index));

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore wait_semaphores[] = { _back_buffer_avaible_semaphore };

		// 指定等待的管线阶段.
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &_cmd_buffer;

		VkSemaphore signal_semaphores[] = { _render_finished_semaphore };
		
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		ReturnIfFalse(vkQueueSubmit(_graphics_queue, 1, &submit_info, _fence) == VK_SUCCESS);

		// 开始交换链的 present.

		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;

		VkSwapchainKHR swapchains[] = { _swapchain };
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swapchains;
		present_info.pImageIndices = &back_buffer_index;

		// 我们可以通过pResults成员变量获取每个交换链的呈现操作是否成功的信息.
		// 由于只使用了一个交换链, 可以直接使用呈现函数的返回值来判断呈现操作是否成功, 没有必要使用 pResults.
		present_info.pResults = nullptr;

		result = vkQueuePresentKHR(_present_queue, &present_info);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window_resized)
		{
			window_resized = false;
			ReturnIfFalse(recreate_swapchain());
		}
		else if (result != VK_SUCCESS)
		{
			LOG_ERROR("vkAcquireNextImageKHR() called failed.");
			return false;
		}


		return true;
	}

	void VulkanBase::clean_up_swapchain()
	{
		for (uint32_t ix = 0; ix < _frame_buffers.size(); ++ix)
		{
			vkDestroyFramebuffer(_device, _frame_buffers[ix], nullptr);
		}


		for (uint32_t ix = 0; ix < _back_buffer_views.size(); ++ix)
		{
			vkDestroyImageView(_device, _back_buffer_views[ix], nullptr);
		}

		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
	}

	bool VulkanBase::update_client_resolution()
	{
		// 这里确保分辨率可以为任意值. (当 surface_capabilities.currentExtent 为 INVALID_SIZE_32, 即分辨率可以为任意值).
		VkSurfaceCapabilitiesKHR surface_capabilities;
		ReturnIfFalse(VK_SUCCESS == vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			_physical_device, 
			_surface, &surface_capabilities
		));
		if (surface_capabilities.currentExtent.width != INVALID_SIZE_32)
		{
			_client_resolution = surface_capabilities.currentExtent;
		}
		else
		{
			int width, height;
 			glfwGetFramebufferSize(_window.get_window(), &width, &height);

			_client_resolution = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
		}
		return true;
	}

    bool VulkanBase::recreate_swapchain() 
	{
        int width = 0, height = 0;
        glfwGetFramebufferSize(_window.get_window(), &width, &height);
        while (width == 0 || height == 0) 
		{
            glfwGetFramebufferSize(_window.get_window(), &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(_device);

        clean_up_swapchain();

		return create_swapchain() && create_frame_buffer();
    }

	uint32_t VulkanBase::get_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags)
	{
		for (uint32_t ix = 0; ix < _memory_properties.memoryTypeCount; ++ix)
		{
			if ((type_filter & (1 << ix)) && (_memory_properties.memoryTypes[ix].propertyFlags & flags) == flags)
			{
				return ix;
			}
		}
		return INVALID_SIZE_32;
	}


	bool VulkanBase::create_vertex_buffer()
	{
		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = sizeof(Vertex) * vertices.size();
		buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		ReturnIfFalse(vkCreateBuffer(_device, &buffer_info, nullptr, &_vertex_buffer) == VK_SUCCESS); 

		VkMemoryRequirements memory_requirements;
		vkGetBufferMemoryRequirements(_device, _vertex_buffer, &memory_requirements);

		VkMemoryAllocateInfo allocate_info{};
		allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocate_info.allocationSize = memory_requirements.size;
		allocate_info.memoryTypeIndex = get_memory_type(
			memory_requirements.memoryTypeBits, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);

		ReturnIfFalse(vkAllocateMemory(_device, &allocate_info, nullptr, &_vertex_buffer_memory) == VK_SUCCESS);
		ReturnIfFalse(vkBindBufferMemory(_device, _vertex_buffer, _vertex_buffer_memory, 0) == VK_SUCCESS);

		void* vertex_data = nullptr;
		vkMapMemory(_device, _vertex_buffer_memory, 0, buffer_info.size, 0, &vertex_data);
		memcpy(vertex_data, vertices.data(), vertices.size() * sizeof(Vertex));
		vkUnmapMemory(_device, _vertex_buffer_memory);

		return true;
	}

	bool VulkanBase::create_index_buffer()
	{
		return true;
	}

}