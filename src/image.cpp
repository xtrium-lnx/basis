#include <basis/image.h>
#include <basis/device.h>

using namespace basis;

vk::raii::Sampler CreateSampler(const vk::raii::Device& device)
{
	auto createInfo = vk::SamplerCreateInfo {
		.magFilter        = vk::Filter::eLinear,
		.minFilter        = vk::Filter::eLinear,
		.mipmapMode       = vk::SamplerMipmapMode::eLinear,
		.addressModeU     = vk::SamplerAddressMode::eMirroredRepeat,
		.addressModeV     = vk::SamplerAddressMode::eMirroredRepeat,
		.addressModeW     = vk::SamplerAddressMode::eMirroredRepeat,
		.anisotropyEnable = true,
		.maxAnisotropy    = 4.0f
	};

	return device.createSampler(createInfo);
}

vk::ImageAspectFlags DeduceVkAspectMask(vk::Format format)
{
	switch (format)
	{
	case vk::Format::eD16Unorm:
	case vk::Format::eD32Sfloat:
	case vk::Format::eX8D24UnormPack32:
		return vk::ImageAspectFlagBits::eDepth;

	case vk::Format::eS8Uint:
		return vk::ImageAspectFlagBits::eStencil;

	case vk::Format::eD16UnormS8Uint:
	case vk::Format::eD24UnormS8Uint:
	case vk::Format::eD32SfloatS8Uint:
		return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;

	default:
		break;
	}

	return vk::ImageAspectFlagBits::eColor;
}

Image::Image(const Device& device, const ImageDesc& desc)
{
	m_aspectMask = DeduceVkAspectMask(desc.format);
	m_format     = desc.format;
	m_usage      = desc.usage;

	auto imageCreateInfo = vk::ImageCreateInfo {
		.flags         = vk::ImageCreateFlags(0),
		.imageType     = desc.type,
		.format        = desc.format,
		.extent        = vk::Extent3D(uint32_t(desc.width), uint32_t(desc.height), uint32_t(desc.depth)),
		.mipLevels     = uint32_t(desc.levels),
		.arrayLayers   = uint32_t(desc.layers),
		.samples       = vk::SampleCountFlagBits::e1,
		.tiling        = vk::ImageTiling::eOptimal,
		.usage         = desc.usage,
		.sharingMode   = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined
	};

	if (desc.initialData)
		imageCreateInfo.usage = imageCreateInfo.usage | vk::ImageUsageFlagBits::eTransferDst;

	auto allocInfo = VmaAllocationCreateInfo {
		.usage         = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	};

	VkImage rawImage;
	vmaCreateImage(
		device.GetAllocator(),
		&*imageCreateInfo,
		&allocInfo,
		&rawImage,
		&m_allocation,
		nullptr
	);

	m_imageRaw = rawImage;
	m_image    = vk::raii::Image(device.GetDevice(), rawImage); // Transfer image ownership to the RAII object

	vk::ImageViewType viewType = ([](vk::ImageType type) {
		switch (type)
		{
		case vk::ImageType::e1D: return vk::ImageViewType::e1D;
		case vk::ImageType::e2D: return vk::ImageViewType::e2D;
		case vk::ImageType::e3D: return vk::ImageViewType::e3D;
		}

		return vk::ImageViewType(-1);
	})(desc.type);

	auto imageViewCreateInfo = vk::ImageViewCreateInfo {
		.image    = m_image,
		.viewType = viewType,
		.format   = desc.format,
		.subresourceRange = {
			.aspectMask     = DeduceVkAspectMask(desc.format),
			.baseMipLevel   = 0,
			.levelCount     = uint32_t(desc.levels),
			.baseArrayLayer = 0,
			.layerCount     = uint32_t(desc.layers)
		}
	};

	m_imageView = vk::raii::ImageView(device.GetDevice(), imageViewCreateInfo);
	m_size      = { desc.width, desc.height, desc.depth };
	m_allocator = device.GetAllocator();

	// TODO: Implement Upload
	//if (desc.initialData)
	//	Upload({}, { desc.width, desc.height, desc.depth }, desc.initialData);

	m_sampler = CreateSampler(device.GetDevice());
}

Image::Image(const Device& device, const ExistingImageDesc& desc)
{
	m_imageRaw   = desc.image;
	m_aspectMask = DeduceVkAspectMask(desc.format);
	m_format     = desc.format;
	m_usage      = desc.usage;
	m_size       = glm::uvec3(desc.extent, 1);

	auto imageViewCreateInfo = vk::ImageViewCreateInfo {
		.image            = desc.image,
		.viewType         = vk::ImageViewType::e2D,
		.format           = desc.format,
		.subresourceRange = { desc.aspectMask, 0, 1, 0, 1 },
	};

	m_imageView = vk::raii::ImageView(device.GetDevice(), imageViewCreateInfo);
}

Image::~Image()
{
	m_imageView.clear();

	if (m_image != nullptr)
	{
		if (m_allocation)
			m_image.release();
		else
			m_image.clear();
	}
	
	if (m_allocation)
		vmaDestroyImage(m_allocator, m_imageRaw, m_allocation);
}

vk::Image Image::GetImage() const
{
	return m_imageRaw;
}

vk::ImageView Image::GetView() const
{
	return *m_imageView;
}

glm::uvec3 Image::Size() const
{
	return m_size;
}

void Image::Upload(const glm::uvec3& offset, const glm::uvec3& size, const void* pixels)
{

}
