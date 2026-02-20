#pragma once

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace basis
{
	class Device;
	class ShaderModule;

	struct PipelineStage
	{
		vk::ShaderModule        module;
		vk::ShaderStageFlagBits stage;
		const char*             entrypoint;
	};

	struct PipelineVertexFormat
	{
		std::vector<vk::VertexInputBindingDescription>   bindings;
		std::vector<vk::VertexInputAttributeDescription> attributes;
	};

	struct AttachmentFormats
	{
		std::vector<vk::Format>   colorFormats;
		std::optional<vk::Format> depthFormat = std::nullopt;
	};

	struct PipelineFormat
	{
		std::vector<PipelineStage>                                    stages;
		PipelineVertexFormat                                          vertexFormat;
		AttachmentFormats                                             attachmentFormats;
		std::vector<std::optional<vk::DescriptorSetLayoutCreateInfo>> descriptorSetLayouts = {};
		std::optional<vk::PushConstantRange>                          pushConstantRange    = std::nullopt;
	};

	class GraphicsPipeline
	{
		vk::raii::PipelineLayout                                  m_pipelineLayout = nullptr;
		vk::raii::Pipeline                                        m_pipeline       = nullptr;
		std::vector<std::optional<vk::raii::DescriptorSetLayout>> m_descriptorSetLayouts;

	public:
		GraphicsPipeline(const Device& device, const PipelineFormat& format);
		~GraphicsPipeline();

		vk::Pipeline       GetPipeline() const;
		vk::PipelineLayout GetLayout() const;

		vk::raii::DescriptorSet CreateDescriptorSet(Device& device, uint32_t set);
	};
}
