#include <basis/buffer.h>
#include <basis/device.h>

using namespace basis;

Buffer::Buffer(Device& device, vk::BufferUsageFlags usage, size_t size, const void* data /* = nullptr */)
{
	auto createInfo = vk::BufferCreateInfo {
		.size  = size,
		.usage = usage | vk::BufferUsageFlagBits::eTransferDst
	};

	auto allocInfo = VmaAllocationCreateInfo {
		.flags = 0,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VkBuffer buffer;
	vmaCreateBuffer(device.GetAllocator(), &*createInfo, &allocInfo, &buffer, &m_allocation, &m_allocInfo);
	m_buffer    = vk::raii::Buffer(device.GetDevice(), buffer); // Transfer buffer ownership to the RAII object
	m_allocator = device.GetAllocator();
	m_size      = size;
	m_usage     = usage;

	if (data)
		Upload(device, data, size);
}

Buffer::~Buffer()
{
	if (m_stagingBuffer != nullptr)
	{
		vmaDestroyBuffer(m_allocator, *m_stagingBuffer, m_stagingAllocation);
		m_stagingBuffer.release();
	}

	vmaDestroyBuffer(m_allocator, *m_buffer, m_allocation);
	m_buffer.release();
}

vk::Buffer Buffer::GetBuffer() const
{
	return *m_buffer;
}

vk::BufferUsageFlags Buffer::GetUsage() const
{
	return m_usage;
}

size_t Buffer::GetSize() const
{
	return m_size;
}

void Buffer::Upload(Device& device, const void* data, size_t size)
{
	auto createInfo = vk::BufferCreateInfo {
		.size  = size,
		.usage = vk::BufferUsageFlagBits::eTransferSrc
	};

	auto allocCreateInfo = VmaAllocationCreateInfo {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VkBuffer stagingBuffer;
	VmaAllocation allocation = VK_NULL_HANDLE;

	VmaAllocationInfo allocationInfo;
	vmaCreateBuffer(m_allocator, &*createInfo, &allocCreateInfo, &stagingBuffer, &allocation, &allocationInfo);

	void* ptr = nullptr;
	vmaMapMemory(m_allocator, allocation, &ptr);
	std::copy_n(reinterpret_cast<const char*>(data), size, reinterpret_cast<char*>(ptr));
	vmaUnmapMemory(m_allocator, allocation);

	auto commandBuffer = device.AcquireCommandBuffer();

	commandBuffer->begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	commandBuffer->copyBuffer(stagingBuffer, m_buffer, { { 0, 0, size } });
	commandBuffer->end();

	device.SubmitAndWait(commandBuffer);
	device.ReleaseCommandBuffer(commandBuffer);

	vmaDestroyBuffer(m_allocator, stagingBuffer, allocation);
}

void Buffer::m_CreatePersistentStagingBuffer(Device& device)
{
	auto createInfo = vk::BufferCreateInfo {
		.size  = m_size,
		.usage = vk::BufferUsageFlagBits::eTransferSrc
	};

	auto allocInfo = VmaAllocationCreateInfo {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VkBuffer buffer;
	vmaCreateBuffer(m_allocator, &*createInfo, &allocInfo, &buffer, &m_stagingAllocation, &m_stagingAllocInfo);
	m_stagingBuffer = vk::raii::Buffer(device.GetDevice(), buffer); // Transfer buffer ownership to the RAII object
}

vk::Semaphore Buffer::UploadAsync(Device& device, const void* data, size_t size, const std::vector<vk::Semaphore>& waitOps /* = {} */)
{
	if (m_stagingBuffer == nullptr)
		m_CreatePersistentStagingBuffer(device);

	void* ptr = nullptr;
	vmaMapMemory(m_allocator, m_stagingAllocation, &ptr);
	std::copy_n(reinterpret_cast<const char*>(data), size, reinterpret_cast<char*>(ptr));
	vmaUnmapMemory(m_allocator, m_stagingAllocation);

	vk::raii::CommandBuffer* commandBuffer = device.AcquireCommandBuffer();
	commandBuffer->begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	commandBuffer->copyBuffer(*m_stagingBuffer, *m_buffer, { {0, 0, size} });
	commandBuffer->end();

	auto result = *device.Submit(commandBuffer, waitOps);
	device.ReleaseCommandBuffer(commandBuffer);
	return result;
}
