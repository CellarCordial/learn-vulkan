#ifndef VULKAN_BASE_H
#define VULKAN_BASE_H


#include "vulkan/vulkan_core.h"
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include <vector>

#include "glfw_window.h"
#include "../source/core/math/common.h"

namespace fantasy
{
	class VulkanBase
	{
	public:
		bool run()
		{
			if (!_window.initialize_window(VkExtent2D{ 1280,720 })) return false;

			initialize();

			while (!_window.should_close())
			{
				glfwPollEvents();
				render_loop();
				_window.title_fps();
			}

			_window.terminate_window();
			destroy();
			return true;
		}

	private:
		bool initialize();
		bool destroy();
		bool render_loop();

		bool create_instance();
		bool check_validation_layer_support();
		bool enumerate_support_extension();

		bool create_debug_utils_messager();
		bool destroy_debug_utils_messager();

		bool pick_physical_device();
		bool find_queue_family(auto& physical_device);

		bool create_device();

	private:
		VkInstance _instance;

		VkDebugUtilsMessengerEXT _debug_messager;
		std::vector<const char*> _validation_layers;
		std::vector<const char*> _extensions;
		VkDebugUtilsMessengerEXT _debug_callback;

		// VkSurfaceKHR 对象是平台无关的, 但它的创建依赖窗口系统.
		// 比如, 在 Windows 系统上, 它的创建需要 HWND 和 HMODULE, 
		// 存在一个叫做 VK_KHR_win32_surface 的 Windows 平台特有扩展, 
		// 用于处理与 Windows 系统窗口交互有关的问题, 
		// 这一扩展也被包含在了 glfwGetRequiredInstanceExtensions 函数获取的扩展列表中.
		// 这里使用 glfw, 故直接用 glfwCreateWindowSurface() 函数获取 surface.
		VkSurfaceKHR _surface;


		VkPhysicalDevice _physical_device = VK_NULL_HANDLE;


		
		// Vulkan 有多种不同类型的队列, 它们属于不同的队列族, 每个队列族的队列只允许执行特定的一部分指令.
		// 创建 VkQueue 逻辑队列时会根据 queue family index 索引进行检索创建.
		// 这里我们需要一个 graphics queue 和一个 present queue.
		struct 
		{
			uint32_t graphics_index = INVALID_SIZE_32;
			uint32_t present_index = INVALID_SIZE_32;
		} _queue_family_index;

		VkDevice _device;
		VkQueue _graphics_queue;
		VkQueue _present_queue;

		GlfwWindow _window;
	};
}








#endif