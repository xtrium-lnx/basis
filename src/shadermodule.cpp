#include <basis/shadermodule.h>
#include <basis/device.h>

using namespace basis;

ShaderModule::ShaderModule(const Device& device, const void* data, size_t size)
{
	auto createInfo = vk::ShaderModuleCreateInfo {
		.codeSize = size,
		.pCode    = reinterpret_cast<const uint32_t*>(data)
	};

	m_shaderModule = vk::raii::ShaderModule(device.GetDevice(), createInfo);
}

ShaderModule::ShaderModule(ShaderModule&& other)
	: m_shaderModule(std::move(other.m_shaderModule))
{
}

ShaderModule::~ShaderModule()
{
	m_shaderModule.clear();
}

vk::ShaderModule ShaderModule::GetModule() const
{
	return *m_shaderModule;
}
