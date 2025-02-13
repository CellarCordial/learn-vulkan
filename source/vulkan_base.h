#ifndef VULKAN_BASE_H
#define VULKAN_BASE_H


#include "core/tools/log.h"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
#include <vector>
#include "glfw_window.h"
#include "../source/core/math/common.h"
#include "../source/core/math/matrix.h"


namespace fantasy
{
	struct Constant
	{
		float4x4 world_matrix;
		float4x4 view_proj;
	};

	struct Vertex
	{
		float2 position;
		float3 color;
		float2 uv;

		static VkVertexInputBindingDescription get_input_binding_description()
		{
			VkVertexInputBindingDescription ret{};
			ret.binding = 0;
			ret.stride = sizeof(Vertex);
			ret.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return ret;

			// VK_VERTEX_INPUT_RATE_VERTEX: 在每个顶点之后移动到下一个数据条目
			// VK_VERTEX_INPUT_RATE_INSTANCE: 在每个实例之后移动到下一个数据条目
		}

		static std::array<VkVertexInputAttributeDescription, 3> get_input_attribute_description()
		{
			std::array<VkVertexInputAttributeDescription, 3> ret{};

			ret[0].binding = 0;
			ret[0].location = 0;
			ret[0].format = VK_FORMAT_R32G32_SFLOAT;
			ret[0].offset = offsetof(Vertex, position);

			ret[1].binding = 0;
			ret[1].location = 1;
			ret[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			ret[1].offset = offsetof(Vertex, color);

			ret[2].binding = 0;
			ret[2].location = 2;
			ret[2].format = VK_FORMAT_R32G32_SFLOAT;
			ret[2].offset = offsetof(Vertex, uv);

			return ret;
		}

	};

	static std::vector<Vertex> vertices = {
		{{-0.5f , -0.5f }, {1.0f , 0.0f , 0.0f}, {1.0f , 0.0f}},
		{{0.5f , -0.5f }, {0.0f , 1.0f , 0.0f}, {0.0f , 0.0f}},
		{{0.5f , 0.5f }, {0.0f , 0.0f , 1.0f}, {0.0f , 1.0f}},
		{{-0.5f , 0.5f }, {1.0f , 1.0f , 1.0f}, {1.0f , 1.0f}}
	};

	static std::vector<uint32_t> indices = {
		0, 1, 2, 2, 3, 0
	};

	class VulkanBase
	{
	public:
		bool run()
		{
			if (!_window.initialize_window(VkExtent2D{ 1280,720 })) return false;

			ReturnIfFalse(initialize());

			while (!_window.should_close())
			{
				glfwPollEvents();
				render_loop();
				_window.title_fps();

			}
			ReturnIfFalse(vkDeviceWaitIdle(_device) == VK_SUCCESS);

			destroy();
			_window.terminate_window();
			return true;
		}

		bool window_resized = false;

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
		bool find_queue_family(const auto& physical_device);

		bool check_device_extension(const auto& physical_device);
		bool check_swapchain_support(const auto& physical_device);
		bool create_render_pass();

		bool create_device();
		bool create_swapchain();
		bool create_pipeline();
		bool create_frame_buffer();
		bool create_command_pool();
		bool create_command_buffer();
		bool record_command(uint32_t frame_buffer_index);
		bool create_sync_objects();

		void clean_up_swapchain();
		bool update_client_resolution();
		bool recreate_swapchain();

		bool create_vertex_buffer();
		bool create_index_buffer();
		bool create_buffer(
			VkDeviceSize size, 
			VkBufferUsageFlags usage, 
			VkMemoryPropertyFlags properties, 
			VkBuffer& buffer, 
			VkDeviceMemory& buffer_memory
		);
		bool create_binding_layout();
		bool create_constant_buffer();
		void update_constant_buffer(uint32_t back_buffer_index);
		bool create_binding_set();
		bool copy_buffer(VkBuffer dst_buffer, VkBuffer src_buffer, VkDeviceSize size);
		bool create_texture();
		bool create_sampler();
		bool draw();

		bool begin_once_command_buffer(VkCommandBuffer& cmd_buffer);
		bool end_once_command_buffer(VkCommandBuffer& cmd_buffer);
		bool texture_transition(VkImage texture, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);

		uint32_t get_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags);
	
	private:
		GlfwWindow _window;

		VkInstance _instance;

		VkDebugUtilsMessengerEXT _debug_messager;
		std::vector<const char*> _validation_layers;
		std::vector<const char*> _instance_extensions;
		VkDebugUtilsMessengerEXT _debug_callback;

		// VkSurfaceKHR 对象是平台无关的, 但它的创建依赖窗口系统.
		// 比如, 在 Windows 系统上, 它的创建需要 HWND 和 HMODULE, 
		// 存在一个叫做 VK_KHR_win32_surface 的 Windows 平台特有扩展, 
		// 用于处理与 Windows 系统窗口交互有关的问题, 
		// 这一扩展也被包含在了 glfwGetRequiredInstanceExtensions 函数获取的扩展列表中.
		// 这里使用 glfw, 故直接用 glfwCreateWindowSurface() 函数获取 surface.
		VkSurfaceKHR _surface;

		std::vector<const char*> _device_extensions;
		VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
		VkPhysicalDeviceMemoryProperties _memory_properties;


		
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


		struct
		{
			VkSurfaceCapabilitiesKHR surface_capabilities;
			std::vector<VkSurfaceFormatKHR> surface_formats;
			std::vector<VkPresentModeKHR> present_modes;
		} _swapchain_info;

		VkExtent2D _client_resolution;
		VkSwapchainKHR _swapchain;
		VkFormat _swapchain_format;
		std::vector<VkImage> _back_buffers;
		std::vector<VkImageView> _back_buffer_views;

		VkViewport _viewport;
		VkRect2D _scissor;

		VkPipelineLayout _layout;
		VkRenderPass _render_pass;
		VkPipeline _graphics_pipeline;

		std::vector<VkFramebuffer> _frame_buffers;

		VkCommandPool _cmd_pool;
		VkCommandBuffer _cmd_buffer;

		VkSemaphore _back_buffer_avaible_semaphore;
		VkSemaphore _render_finished_semaphore;
		VkFence _fence;

		VkBuffer _vertex_buffer;
		VkDeviceMemory _vertex_buffer_memory;
		VkBuffer _index_buffer;
		VkDeviceMemory _index_buffer_memory;

		VkDescriptorSetLayout _binding_layout;
		VkDescriptorPool _descriptor_pool;
		std::vector<VkDescriptorSet> _binding_sets;
		std::vector<VkBuffer> _constant_buffers;
		std::vector<VkDeviceMemory> _constant_buffer_memorys;
		std::vector<void*> _constant_buffer_mapped_datas;

		VkImage _test_texture;
		VkDeviceMemory _test_texture_memory;
		VkImageView _test_texture_view;

		VkSampler _linear_wrap_sampler;
	};
}








#endif