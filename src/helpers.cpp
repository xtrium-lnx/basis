#include <basis/helpers.h>
#include <basis/buffer.h>

#include <fstream>

void basis::Transition(vk::raii::CommandBuffer& cb, vk::ImageLayout from, vk::ImageLayout to, const vk::Image& image, bool isDepth /* = false */, bool force /* = false */)
{
	// FIXME: Maybe concurrency issues when working with the same image in multiple command buffers ?
	if (from == to && !force)
		return;

	auto barrier = vk::ImageMemoryBarrier2 {
		.srcStageMask        = vk::PipelineStageFlagBits2::eTopOfPipe,
		.srcAccessMask       = vk::AccessFlagBits2::eNone,
		.dstStageMask        = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.dstAccessMask       = vk::AccessFlagBits2::eColorAttachmentWrite,
		.oldLayout           = from,
		.newLayout           = to,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image               = image,
		.subresourceRange    = {
			.aspectMask        = isDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
			.baseMipLevel      = 0,
			.levelCount        = 1, // FIXME: Needs to work for multiple mip levels too
			.baseArrayLayer    = 0,
			.layerCount        = 1  // FIXME: Needs to work for array images too
		}
	};

	auto dependencyInfo = vk::DependencyInfo {
		.dependencyFlags         = {},
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers    = &barrier
	};

	cb.pipelineBarrier2(dependencyInfo);
}

vk::ImageLayout basis::ClearColor(vk::raii::CommandBuffer& cb, const vk::Image& image, vk::ImageLayout previousLayout, const glm::vec4& color)
{
	auto clearRange = vk::ImageSubresourceRange {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.levelCount = vk::RemainingMipLevels,
		.layerCount = vk::RemainingArrayLayers
	};

	Transition(cb, previousLayout, vk::ImageLayout::eTransferDstOptimal, image, false, true);
	cb.clearColorImage(image, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(0.1f, 0.2f, 0.3f, 1.0f), clearRange);

	return vk::ImageLayout::eTransferDstOptimal;
}

template<>
void basis::BindToDescriptorSet<basis::Buffer>(Device& device, const vk::raii::DescriptorSet& ds, uint32_t binding, const basis::Buffer& target)
{
	auto bufferInfo = vk::DescriptorBufferInfo {
		.buffer = target.GetBuffer(),
		.range  = static_cast<vk::DeviceSize>(target.GetSize())
	};

	auto descriptorWrite = vk::WriteDescriptorSet {
		.dstSet          = ds,
		.dstBinding      = binding,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType  = uint32_t(target.GetUsage() & vk::BufferUsageFlagBits::eUniformBuffer) ? vk::DescriptorType::eUniformBuffer : vk::DescriptorType::eStorageBuffer,
		.pBufferInfo     = &bufferInfo
	};

	device.GetDevice().updateDescriptorSets({ descriptorWrite }, {});
}
