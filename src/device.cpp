#define VMA_IMPLEMENTATION
#include <basis/device.h>
#include <basis/window.h>
#include <basis/image.h>

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <print>
#include <ranges>

using namespace basis;

const std::vector<char const*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

vk::raii::Context gVkRaiiContext;

/**
 * Creates a Vulkan 1.4 instance.
 * Will automatically enable validation layers in debug mode.
 * Automatically fetches the platform-appropriate instance extensions for GLFW / surface creation.
 */
vk::raii::Instance Device::m_InitInstance()
{
	const auto appInfo = vk::ApplicationInfo{
		.pApplicationName   = "Basis App",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName        = "Basis",
		.engineVersion      = VK_MAKE_VERSION(0, 1, 0),
		.apiVersion         = vk::ApiVersion14
	};

	std::vector<char const*> requiredLayers;
#ifndef NDEBUG
	requiredLayers.assign(validationLayers.begin(), validationLayers.end());
#endif /* NDEBUG */

	const auto layerProperties = gVkRaiiContext.enumerateInstanceLayerProperties();
	if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const& requiredLayer) { return std::ranges::none_of(layerProperties, [requiredLayer](auto const& layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; }); }))
		throw std::runtime_error("One or more required layers are not supported !");

	uint32_t glfwExtensionCount = 0;
	auto     glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	auto     extensionProperties = gVkRaiiContext.enumerateInstanceExtensionProperties();

	std::vector<const char*> activeExtensions;
	for (uint32_t i = 0; i < glfwExtensionCount; ++i)
		activeExtensions.push_back(glfwExtensions[i]);

	for (uint32_t i = 0; i < glfwExtensionCount; ++i)
		if (std::ranges::none_of(extensionProperties, [glfwExtension = glfwExtensions[i]](auto const& extensionProperty) { return strcmp(extensionProperty.extensionName, glfwExtension) == 0; }))
			throw std::runtime_error("Required GLFW extensions not supported");

	auto instanceCreateInfo = vk::InstanceCreateInfo {
		.pApplicationInfo        = &appInfo,
		.enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
		.ppEnabledLayerNames     = requiredLayers.data(),
		.enabledExtensionCount   = uint32_t(activeExtensions.size()),
		.ppEnabledExtensionNames = activeExtensions.data()
	};

	std::println("Basis: Initialized Vulkan instance");
	return vk::raii::Instance(gVkRaiiContext, instanceCreateInfo);
}

/**
 * Picks a physical device.
 * Will reject all physical devices not supporting VK 1.4
 * Will accept the first appropriate discrete GPU. Otherwise, returns a best-fit.
 */
vk::raii::PhysicalDevice Device::m_PickPhysicalDevice()
{
	auto devices = m_instance.enumeratePhysicalDevices();
	if (devices.empty())
		return nullptr;

	vk::raii::PhysicalDevice result = nullptr;

	for (const auto& d : devices)
	{
		auto deviceProperties = d.getProperties();

		if (deviceProperties.apiVersion < VK_API_VERSION_1_4)
			continue;

		if (deviceProperties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu)
			return d;
		
		result = d;
	}

	return result;
}

/**
 * Picks a queue family index from the specified queue flags.
 */
uint32_t Device::m_PickQueueFamily(vk::QueueFlags requestedQueueFlags)
{
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();
	auto graphicsQueueFamilyProperty = std::find_if(
		queueFamilyProperties.begin(),
		queueFamilyProperties.end(),
		[&](vk::QueueFamilyProperties const& qfp) { return qfp.queueFlags & requestedQueueFlags; }
	);

	auto result = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
	std::println("Basis: Picked queue family = {}", result);
	return result;
}

/**
 * Creates a SurfaceKHR from a GLFWwindow.
 * Ownership of the SurfaceKHR is transferred to the vk::raii::SurfaceKHR
 * on return for easier lifetime management
 */
vk::raii::SurfaceKHR Device::m_CreateGlfwWindowSurface(GLFWwindow* window)
{
	VkSurfaceKHR _surface;
	if (glfwCreateWindowSurface(*m_instance, window, nullptr, &_surface) != VK_SUCCESS)
		throw std::runtime_error("Failed to create GLFW window surface");

	return vk::raii::SurfaceKHR(m_instance, _surface);
}

/**
 * Creates a device from the specified physical device.
 * Note: In Vulkan, creating a device also creates its associated queues (cf. https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html#devsandqueues-queue-creation)
 * so we also need to specify which queue family to use to create the "main" queue (we only have one in this case)
 */
vk::raii::Device Device::m_CreateDevice(uint32_t graphicsQueueFamily)
{
	float queuePriority = 0.0f;
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();
	auto deviceQueueCreateInfo = vk::DeviceQueueCreateInfo{
		.queueFamilyIndex = graphicsQueueFamily,
		.queueCount       = 1,
		.pQueuePriorities = &queuePriority
	};

	vk::StructureChain<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan12Features,
		vk::PhysicalDeviceVulkan13Features
	> featureChain = {
		{.features = { .samplerAnisotropy = true } },
		{.bufferDeviceAddress = true },
		{.synchronization2 = true, .dynamicRendering = true },
	};

	std::vector<const char*> deviceExtensions = {
		vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName
	};

	auto deviceCreateInfo = vk::DeviceCreateInfo {
		.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		.queueCreateInfoCount    = 1,
		.pQueueCreateInfos       = &deviceQueueCreateInfo,
		.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data()
	};

	return vk::raii::Device(m_physicalDevice, deviceCreateInfo);
}

/**
 * Just like commands, descriptors also come in pools.
 * While you CAN have multiple descriptor pools allocated, most of the time, just having one per device is more than enough.
 * In our case, we're just being generous with how many things we want in there to give ourselves some room.
 */
vk::raii::DescriptorPool Device::m_CreateDescriptorPool()
{
	std::vector<vk::DescriptorPoolSize> poolSizes = {
		{ vk::DescriptorType::eUniformBuffer,            1024 },
		{ vk::DescriptorType::eStorageBuffer,            1024 },
		{ vk::DescriptorType::eCombinedImageSampler,     2048 },
		{ vk::DescriptorType::eSampledImage,             1024 },
		{ vk::DescriptorType::eStorageImage,             512  },
		{ vk::DescriptorType::eUniformTexelBuffer,       256  },
		{ vk::DescriptorType::eStorageTexelBuffer,       256  },
		{ vk::DescriptorType::eSampler,                  512  },
		{ vk::DescriptorType::eInputAttachment,          256  }
	};

	auto createInfo = vk::DescriptorPoolCreateInfo {
		.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets       = 2048,
		.poolSizeCount = uint32_t(poolSizes.size()),
		.pPoolSizes    = poolSizes.data()
	};

	return m_device.createDescriptorPool(createInfo);	
}

/**
 * Creates a swapchain and its associated images from a specified physical device, (logical) device and surface.
 * The GLFWwindow is passed to retrieve the framebuffer's size.
 * Relaxed FIFO presentation mode is preferred, but if unavailable, we fall back to plain FIFO which is available everywhere.
 */
std::pair<vk::raii::SwapchainKHR, std::vector<std::unique_ptr<Image>>> Device::CreateSwapchain(GLFWwindow* window)
{
	auto surfaceCaps           = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
	auto availableFormats      = m_physicalDevice.getSurfaceFormatsKHR(m_surface);
	auto availablePresentModes = m_physicalDevice.getSurfacePresentModesKHR(m_surface);

	auto swapSurfaceFormat = ([](const decltype(availableFormats)& formats) {
		for (const auto& availableFormat : formats)
			if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
				return availableFormat;
		return formats[0];
	})(availableFormats);

	auto swapExtent = ([&window](const vk::SurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		return vk::Extent2D {
			std::clamp<uint32_t>(width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};
	})(surfaceCaps);

	auto swapPresentMode = ([](const decltype(availablePresentModes)& modes) {
		for (const auto& availablePresentMode : modes)
			if (availablePresentMode == vk::PresentModeKHR::eFifoRelaxed)
				return availablePresentMode;
		return vk::PresentModeKHR::eFifo;
	})(availablePresentModes);

	auto minImageCount = std::clamp(3u, surfaceCaps.minImageCount, surfaceCaps.maxImageCount);

	auto swapchainCreateInfo = vk::SwapchainCreateInfoKHR {
		.flags            = vk::SwapchainCreateFlagsKHR(0),
		.surface          = m_surface,
		.minImageCount    = minImageCount,
		.imageFormat      = swapSurfaceFormat.format,
		.imageColorSpace  = swapSurfaceFormat.colorSpace,
		.imageExtent      = swapExtent,
		.imageArrayLayers = 1,
		.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.preTransform     = surfaceCaps.currentTransform,
		.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode      = swapPresentMode,
		.clipped          = true,
		.oldSwapchain     = nullptr
	};

	auto swapchain = vk::raii::SwapchainKHR(m_device, swapchainCreateInfo);

	auto swapchainImages = swapchain.getImages() | std::views::transform([this, swapSurfaceFormat, swapExtent](const vk::Image& image) {
		auto desc = ExistingImageDesc {
			.image      = image,
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.format     = swapSurfaceFormat.format,
			.usage      = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
			.extent     = glm::uvec2(swapExtent.width, swapExtent.height)
		};

		return std::make_unique<Image>(*this, desc);
	}) | std::ranges::to<std::vector>();

	return { std::move(swapchain), std::move(swapchainImages) };
}

Device::Device(const Window& window)
{
	m_instance       = m_InitInstance();
	m_physicalDevice = m_PickPhysicalDevice();
	if (m_physicalDevice != nullptr)
		std::println("Basis: Picked PhysicalDevice = {}", static_cast<std::string_view>(m_physicalDevice.getProperties().deviceName));
	else
		throw std::runtime_error("Unable to pick a physical device.");

	m_mainQueueFamily = m_PickQueueFamily(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
	m_surface         = m_CreateGlfwWindowSurface(window.NativeHandle());
	m_device          = m_CreateDevice(m_mainQueueFamily);
	m_mainQueue       = vk::raii::Queue(m_device, m_mainQueueFamily, 0); // Fetches a queue from the device (the only one it has in this case)
	std::tie(m_swapchain, m_swapchainImages) = CreateSwapchain(window.NativeHandle());

	auto poolInfo = vk::CommandPoolCreateInfo {
		.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = m_mainQueueFamily
	};
	m_commandPool = vk::raii::CommandPool(m_device, poolInfo);

	auto vmaCreateInfo = VmaAllocatorCreateInfo {
		.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice   = *m_physicalDevice,
		.device           = *m_device,
		.instance         = *m_instance,
		.vulkanApiVersion = VK_MAKE_VERSION(1, 4, 0)
	};
	vmaCreateAllocator(&vmaCreateInfo, &m_allocator);

	m_frameData = m_swapchainImages | std::views::transform([&](const std::unique_ptr<Image>&) {
		auto allocInfo = vk::CommandBufferAllocateInfo {
			.commandPool        = *m_commandPool,
			.level              = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1
		};

		return FrameData {
			.presentCompleteSemaphore = vk::raii::Semaphore(m_device, vk::SemaphoreCreateInfo()),
			.renderFinishedSemaphore  = vk::raii::Semaphore(m_device, vk::SemaphoreCreateInfo()),
			.inFlightFence            = vk::raii::Fence(m_device, { .flags = vk::FenceCreateFlagBits::eSignaled })
		};
	}) | std::ranges::to<std::vector>();

	m_descriptorPool = m_CreateDescriptorPool();
}

Device::~Device()
{
	m_device.waitIdle();

	m_commandBuffers.clear();
	m_descriptorPool.clear();
	m_frameData.clear();
	vmaDestroyAllocator(m_allocator);
	m_commandPool.clear();
	m_swapchainImages.clear();
	m_swapchain.clear();
	m_mainQueue.clear();
	m_device.clear();
	m_surface.clear();
	m_physicalDevice.clear();
	m_instance.clear();
}

const vk::raii::Device& Device::GetDevice() const
{
	return m_device;
}

const VmaAllocator& Device::GetAllocator() const
{
	return m_allocator;
}


uint32_t Device::GetMainQueueFamily() const
{
	return m_mainQueueFamily;
}

const vk::raii::DescriptorPool& Device::GetDescriptorPool() const
{
	return m_descriptorPool;
}

vk::raii::CommandBuffer* Device::AcquireCommandBuffer()
{
	std::scoped_lock lock(m_commandBufferMutex);

	for (auto& [_, cb] : m_commandBuffers)
	{
		bool isFree = cb.isCpuFree && (cb.isGpuFree.getStatus() == vk::Result::eSuccess);

		if (isFree)
		{
			cb.isCpuFree = false;
			return &cb.commandBuffer;
		}
	}

	auto allocInfo = vk::CommandBufferAllocateInfo {
		.commandPool        = *m_commandPool,
		.level              = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};

	auto cb    = std::move(vk::raii::CommandBuffers(m_device, allocInfo).front());
	auto cbPtr = *cb;

	m_commandBuffers[cbPtr] = CommandBuffer {
		.commandBuffer = std::move(cb),
		.semaphore     = vk::raii::Semaphore(m_device, vk::SemaphoreCreateInfo()),
		.isGpuFree     = vk::raii::Fence(m_device, {.flags = vk::FenceCreateFlagBits::eSignaled }),
		.isCpuFree     = false
	};

	return &m_commandBuffers[cbPtr].commandBuffer;
}

void Device::ReleaseCommandBuffer(vk::raii::CommandBuffer*& commandBuffer)
{
	std::scoped_lock lock(m_commandBufferMutex);

	if (m_commandBuffers.contains(**commandBuffer))
		m_commandBuffers[**commandBuffer].isCpuFree = true;

	commandBuffer = nullptr;
}

std::optional<vk::Semaphore> Device::Submit(vk::raii::CommandBuffer* cb, const std::vector<vk::Semaphore>& waitOps, std::optional<vk::Semaphore> signalOp /* = std::nullopt */)
{
	auto& cbData = m_commandBuffers[**cb];

	auto waitDestinationStageMasks = waitOps | std::views::transform([](const vk::Semaphore&) {
		return vk::PipelineStageFlags(vk::PipelineStageFlagBits::eAllCommands);
	}) | std::ranges::to<std::vector>();

	const auto submitInfo = vk::SubmitInfo {
		   .waitSemaphoreCount   = static_cast<uint32_t>(waitOps.size()),
		   .pWaitSemaphores      = waitOps.data(),
		   .pWaitDstStageMask    = waitDestinationStageMasks.data(),
		   .commandBufferCount   = 1,
		   .pCommandBuffers      = &*cbData.commandBuffer,
		   .signalSemaphoreCount = 1,
		   .pSignalSemaphores    = signalOp ? &*signalOp : &*cbData.semaphore,
	};

	m_device.resetFences({ cbData.isGpuFree });
	m_mainQueue.submit(submitInfo, cbData.isGpuFree);

	if (signalOp)
		return std::nullopt;

	return *cbData.semaphore;
}

void Device::SubmitAndWait(vk::raii::CommandBuffer* cb, const std::vector<vk::Semaphore>& waitOps)
{
	auto& cbData = m_commandBuffers[**cb];

	auto waitDestinationStageMasks = waitOps | std::views::transform([](const vk::Semaphore&) {
		return vk::PipelineStageFlags(vk::PipelineStageFlagBits::eAllCommands);
	}) | std::ranges::to<std::vector>();

	const auto submitInfo = vk::SubmitInfo {
		   .waitSemaphoreCount   = static_cast<uint32_t>(waitOps.size()),
		   .pWaitSemaphores      = waitOps.data(),
		   .pWaitDstStageMask    = waitDestinationStageMasks.data(),
		   .commandBufferCount   = 1,
		   .pCommandBuffers      = &*cbData.commandBuffer,
		   .signalSemaphoreCount = 0,
		   .pSignalSemaphores    = nullptr,
	};

	m_device.resetFences({ cbData.isGpuFree });
	m_mainQueue.submit(submitInfo, cbData.isGpuFree);
	if (m_device.waitForFences({ cbData.isGpuFree }, true, UINT64_MAX) != vk::Result::eSuccess)
		throw std::runtime_error("Error while waiting for fence");
}

void Device::WaitForCompletion(vk::raii::CommandBuffer* cb)
{
	auto& cbData = m_commandBuffers[**cb];
	if (m_device.waitForFences({ cbData.isGpuFree }, true, UINT64_MAX) != vk::Result::eSuccess)
		throw std::runtime_error("Error while waiting for fence");
}

void Device::WaitForIdle()
{
	m_device.waitIdle();
}

void Device::Defer(std::move_only_function<void()>&& action)
{
	m_frameData[m_currentFrame].deferredActions.push_back(std::move(action));
}

void Device::FlushDeferred()
{
	for (auto& fd : m_frameData)
	{
		for (auto& a : fd.deferredActions)
			a();

		fd.deferredActions.clear();
	}
}

std::tuple<Image&, vk::Semaphore, vk::Semaphore> Device::AcquireNextFrame()
{
	while (m_device.waitForFences({ m_frameData[m_currentFrame].inFlightFence }, true, UINT64_MAX) != vk::Result::eSuccess) {}
	m_device.resetFences({ m_frameData[m_currentFrame].inFlightFence });

	for (auto& action : m_frameData[m_currentFrame].deferredActions)
		action();

	m_frameData[m_currentFrame].deferredActions.clear();

	vk::Result result;
	std::tie(result, m_currentImageIndex) = m_swapchain.acquireNextImage(UINT64_MAX, m_frameData[m_currentFrame].presentCompleteSemaphore, nullptr);
	if (result != vk::Result::eSuccess)
		throw std::runtime_error("No eSuccess at acquire. Check Suboptimal / OutOfDate (recreation not implemented) ?");

	return { *m_swapchainImages[m_currentImageIndex], *m_frameData[m_currentFrame].presentCompleteSemaphore, *m_frameData[m_currentImageIndex].renderFinishedSemaphore };
}

void Device::Present(const std::vector<vk::Semaphore>& waitOps)
{
	const auto presentInfo = vk::PresentInfoKHR {
		.waitSemaphoreCount = static_cast<uint32_t>(waitOps.size()),
		.pWaitSemaphores    = waitOps.data(),
		.swapchainCount     = 1,
		.pSwapchains        = &*m_swapchain,
		.pImageIndices      = &m_currentImageIndex
	};

	if (m_mainQueue.presentKHR(presentInfo) != vk::Result::eSuccess)
		throw std::runtime_error("No eSuccess while presenting. Check Suboptimal / OutOfDate (recreation not implemented) ?");

	m_mainQueue.submit({}, m_frameData[m_currentFrame].inFlightFence);
	++m_currentFrame %= m_frameData.size();
}
