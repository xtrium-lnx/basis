#pragma once

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vma/vk_mem_alloc.h>

#include <glm/glm.hpp>

namespace basis
{
	class Device;

	struct ImageDesc
	{
		vk::ImageType       type;
		vk::Format          format;
		size_t              width       = 0;
		size_t              height      = 0;
		size_t              depth       = 1;
		vk::ImageUsageFlags usage       = vk::ImageUsageFlags(0);
		size_t              levels      = 1;
		size_t              layers      = 1;

		const void*         initialData = nullptr;
	};

	struct ExistingImageDesc
	{
		vk::Image            image;
		vk::ImageAspectFlags aspectMask;
		vk::Format           format;
		vk::ImageUsageFlags  usage;
		glm::uvec2           extent;
	};

	class Image
	{
		vk::raii::Image      m_image      = nullptr;
		vk::Image            m_imageRaw;

		VmaAllocator         m_allocator  = VK_NULL_HANDLE;
		VmaAllocation        m_allocation = VK_NULL_HANDLE;

		vk::raii::ImageView  m_imageView  = nullptr;
		vk::raii::Sampler    m_sampler    = nullptr;

		vk::ImageAspectFlags m_aspectMask;
		vk::Format           m_format;
		vk::ImageUsageFlags  m_usage;
		glm::uvec3           m_size;

	public:
		Image(const Device& device, const ImageDesc& desc);
		Image(const Device& device, const ExistingImageDesc& desc);
		~Image();

		vk::Image     GetImage() const;
		vk::ImageView GetView() const;

		glm::uvec3 Size() const;
		void       Upload(const glm::uvec3& offset, const glm::uvec3& size, const void* pixels);
	};
}
