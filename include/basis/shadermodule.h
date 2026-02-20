#pragma once

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace basis
{
	class Device;
	class ShaderModule
	{
		vk::raii::ShaderModule m_shaderModule = nullptr;

	public:
		ShaderModule(const Device& device, const void* data, size_t size);
		ShaderModule(ShaderModule&& other);
		~ShaderModule();

		vk::ShaderModule GetModule() const;
	};
}
