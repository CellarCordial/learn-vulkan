#include "vulkan_base.h"
#include "../source/core/tools/log.h"
#include <minwindef.h>
#include <set>
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
		pick_physical_device();
		create_device();
		return true;
	}


	bool VulkanBase::destroy()
	{
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
		_extensions.insert(_extensions.end(), glfw_extensions, glfw_extensions + glfw_extension_count);

#ifdef DEBUG
		// LunarG 的 Vulkan SDK 允许我们通过 VKLAYER_KHRONOS_validation 来隐式地开启所有可用的校验层.
		_validation_layers.push_back("VK_LAYER_KHRONOS_validation");

		// 检查系统是否支持 _validation_layers 内的所有校验层, 
		// 现在只有 VK_LAYER_KHRONOS_validation 一个校验层等待检查.
		ReturnIfFalse(check_validation_layer_support());
		
		instance_info.enabledLayerCount = static_cast<uint32_t>(_validation_layers.size());
		instance_info.ppEnabledLayerNames = _validation_layers.data();


		_extensions.push_back("VK_EXT_debug_utils");
#endif

		instance_info.enabledExtensionCount = static_cast<uint32_t>(_extensions.size());
		instance_info.ppEnabledExtensionNames = _extensions.data();

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

			if (
				(
					properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
					properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU 
				) && 
				features.geometryShader && 
				find_queue_family(device)
			)
			{
				_physical_device = device;
				break;
			}
		}

		return true;
	}

	bool VulkanBase::find_queue_family(auto& physical_device)
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

			if (_queue_family_index.graphics_index != INVALID_SIZE_32 && _queue_family_index.graphics_index != INVALID_SIZE_32) break;
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

		// VK_KHR_surface 是一个实例扩展, 而不是设备扩展, 所以这里我们把 extension count 直接设为 0.
		device_create_info.enabledExtensionCount = 0;
#if DEBUG
		device_create_info.ppEnabledLayerNames = _validation_layers.data();
		device_create_info.enabledLayerCount = static_cast<uint32_t>(_validation_layers.size());
#endif

		ReturnIfFalse(vkCreateDevice(_physical_device, &device_create_info, nullptr, &_device) == VK_SUCCESS);
		vkGetDeviceQueue(_device, _queue_family_index.graphics_index, 0, &_graphics_queue);
		vkGetDeviceQueue(_device, _queue_family_index.present_index, 0, &_present_queue);
		
		return true;  
	}

}