#pragma once

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vma/vk_mem_alloc.h>

namespace basis
{
	class Device;

	class Buffer
	{
		vk::raii::Buffer     m_buffer     = nullptr;
		VmaAllocation        m_allocation = VK_NULL_HANDLE;
		VmaAllocationInfo    m_allocInfo;
		VmaAllocator         m_allocator  = VK_NULL_HANDLE;
		size_t               m_size       = 0;
		vk::BufferUsageFlags m_usage;

		vk::raii::Buffer     m_stagingBuffer     = nullptr;
		VmaAllocation        m_stagingAllocation = VK_NULL_HANDLE;
		VmaAllocationInfo    m_stagingAllocInfo;
		VmaAllocator         m_stagingAllocator  = VK_NULL_HANDLE;
		void m_CreatePersistentStagingBuffer(Device& device);

	public:
		Buffer(Device& device, vk::BufferUsageFlags usage, size_t size, const void* data = nullptr);
		~Buffer();

		vk::Buffer           GetBuffer() const;
		vk::BufferUsageFlags GetUsage()  const;
		size_t               GetSize()   const;

		void Upload(Device& device, const void* data, size_t size);
		vk::Semaphore UploadAsync(Device& device, const void* data, size_t size, const std::vector<vk::Semaphore>& waitOps = {});

		// Helpers for more comfortable buffer creation
		template<typename T, size_t N>
		Buffer(Device& device, vk::BufferUsageFlags usage, const std::array<T, N>& data)
			: Buffer(device, usage, N * sizeof(T), data.data())
		{}

		template<typename T>
		Buffer(Device& device, vk::BufferUsageFlags usage, const T& data)
			: Buffer(device, usage, sizeof(T), &data)
		{}

		template<typename T>
		Buffer(Device& device, vk::BufferUsageFlags usage, const std::vector<T>& data)
			: Buffer(device, usage, data.size() * sizeof(T), data.data())
		{}

		// Helpers for more comfortable buffer uploads
		template<typename T>
		void Upload(Device& device, const T& data)
		{ Upload(device, &data, sizeof(T)); }

		template<typename T>
		vk::Semaphore UploadAsync(Device& device, const T& data, const std::vector<vk::Semaphore>& waitOps = {})
		{ return UploadAsync(device, &data, sizeof(T), waitOps); }
	};
}
