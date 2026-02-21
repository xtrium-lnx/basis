#pragma once

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vma/vk_mem_alloc.h>

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

namespace basis
{
	class Window;
	class Image;

	class Device
	{		
		struct FrameData
		{
			vk::raii::Semaphore                          presentCompleteSemaphore    = nullptr;
			vk::raii::Semaphore                          renderFinishedSemaphore     = nullptr;
			vk::raii::Fence                              inFlightFence               = nullptr;

			std::vector<std::move_only_function<void()>> deferredActions;
		};

		vk::raii::Instance                  m_instance          = nullptr;
		vk::raii::PhysicalDevice            m_physicalDevice    = nullptr;
		vk::raii::SurfaceKHR                m_surface           = nullptr;
		vk::raii::Device                    m_device            = nullptr;

		uint32_t                            m_mainQueueFamily;
		vk::raii::Queue                     m_mainQueue         = nullptr;

		vk::raii::SwapchainKHR              m_swapchain         = nullptr;
		std::vector<std::unique_ptr<Image>> m_swapchainImages;

		vk::raii::DescriptorPool            m_descriptorPool    = nullptr;
		vk::raii::CommandPool               m_commandPool       = nullptr;
		std::vector<FrameData>              m_frameData;

		VmaAllocator                        m_allocator         = VK_NULL_HANDLE;
		size_t                              m_currentFrame      = 0;
		uint32_t                            m_currentImageIndex = 0;

		struct CommandBuffer
		{
			vk::raii::CommandBuffer commandBuffer = nullptr;
			vk::raii::Semaphore     semaphore     = nullptr;
			vk::raii::Fence         isGpuFree     = nullptr;
			bool                    isCpuFree;
		};
		std::unordered_map<VkCommandBuffer, CommandBuffer> m_commandBuffers;
		std::mutex                                         m_commandBufferMutex;

		
		vk::raii::Instance                                                     m_InitInstance();
		vk::raii::PhysicalDevice                                               m_PickPhysicalDevice();
		uint32_t                                                               m_PickQueueFamily(vk::QueueFlags requestedQueueFlags);
		vk::raii::SurfaceKHR                                                   m_CreateGlfwWindowSurface(GLFWwindow* window);
		vk::raii::Device                                                       m_CreateDevice(uint32_t graphicsQueueFamily);
		vk::raii::DescriptorPool                                               m_CreateDescriptorPool();
		std::pair<vk::raii::SwapchainKHR, std::vector<std::unique_ptr<Image>>> m_CreateSwapchain(GLFWwindow* window);

	public:
		explicit Device(const Window& window);
		~Device();

		const vk::raii::Device& GetDevice() const;
		const VmaAllocator& GetAllocator() const;
		uint32_t GetMainQueueFamily() const;
		const vk::raii::DescriptorPool& GetDescriptorPool() const;

		vk::raii::CommandBuffer* AcquireCommandBuffer();
		void ReleaseCommandBuffer(vk::raii::CommandBuffer*& commandBuffer);

		std::optional<vk::Semaphore> Submit(vk::raii::CommandBuffer* cb, const std::vector<vk::Semaphore>& waitOps = {}, std::optional<vk::Semaphore> signalOp = std::nullopt);
		void SubmitAndWait(vk::raii::CommandBuffer* cb, const std::vector<vk::Semaphore>& waitOps = {});
		void WaitForCompletion(vk::raii::CommandBuffer* cb);
		void WaitForIdle();

		void Defer(std::move_only_function<void()> action);
		void FlushDeferred();

		std::tuple<Image&, vk::Semaphore, vk::Semaphore> AcquireNextFrame();
		void Present(const std::vector<vk::Semaphore>& waitOps);
	};
}
