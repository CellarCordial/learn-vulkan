#include "vulkan_base.h"
#include "../source/core/tools/log.h"
#include <cstdint>
#include <minwindef.h>
#include <set>
#include <vector>
#include "core/math/common.h"
#include "vulkan/vulkan_core.h"

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

	bool VulkanBase::initialize()
	{
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
		return true;
	}


	bool VulkanBase::destroy()
	{
#ifdef DEBUG
		ReturnIfFalse(destroy_debug_utils_messager());
#endif
		for (const auto& view : _back_buffer_views) vkDestroyImageView(_device, view, nullptr);
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyDevice(_device, nullptr);
		vkDestroyInstance(_instance, nullptr);
		return true;
	}

	bool VulkanBase::render_loop()
	{
		return true;
	}

	bool VulkanBase::create_instance()
	{
		// Instance info 需要 layers 和 extensions.
		VkInstanceCreateInfo instance_info{};
		instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Vulkan Test";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);
		
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

		for (const auto& present_mode : _swapchain_info.present_modes)
		{
			if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) { swapchain_create_info.presentMode = present_mode; break; }
			else if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) { swapchain_create_info.presentMode = present_mode; break; }
			else if (present_mode == VK_PRESENT_MODE_FIFO_KHR) { swapchain_create_info.presentMode = present_mode; break; }
		}

		// 确定缓冲区分辨率.
		if (_swapchain_info.surface_capabilities.currentExtent.width != INVALID_SIZE_32)
		{
			swapchain_create_info.imageExtent = _swapchain_info.surface_capabilities.currentExtent;
		}
		else
		{
			VkExtent2D extent = { CLIENT_WIDTH, CLIENT_HEIGHT };
			swapchain_create_info.imageExtent.width = std::max(
				_swapchain_info.surface_capabilities.minImageExtent.width, 
				std::min(
					extent.width, 
					_swapchain_info.surface_capabilities.maxImageExtent.width
				)
			);
			swapchain_create_info.imageExtent.height = std::max(
				_swapchain_info.surface_capabilities.minImageExtent.height, 
				std::min(
					extent.height, 
					_swapchain_info.surface_capabilities.maxImageExtent.height
				)
			);
		}

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

	bool VulkanBase::load_shader()
	{
		VkShaderModuleCreateInfo create_info{};
		
		return true;
	}
}