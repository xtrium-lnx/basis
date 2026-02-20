#include <basis/graphicspipeline.h>
#include <basis/device.h>

#include <ranges>

using namespace basis;

GraphicsPipeline::GraphicsPipeline(const Device& device, const PipelineFormat& format)
{
	auto colorBlendAttachments = format.attachmentFormats.colorFormats | std::views::transform([](const vk::Format&) {
		return vk::PipelineColorBlendAttachmentState { .colorWriteMask = vk::FlagTraits<vk::ColorComponentFlagBits>::allFlags };
	}) | std::ranges::to<std::vector>();

	auto dynamicStates = std::vector<vk::DynamicState> { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	auto dynamicStateCreateInfo = vk::PipelineDynamicStateCreateInfo {
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates    = dynamicStates.data()
	};

	auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo { .topology = vk::PrimitiveTopology::eTriangleList };
	auto viewportState = vk::PipelineViewportStateCreateInfo      { .viewportCount = 1, .scissorCount = 1 };
	auto rasterizer    = vk::PipelineRasterizationStateCreateInfo { .lineWidth = 1.0f };
	auto multisampling = vk::PipelineMultisampleStateCreateInfo   {};
	
	auto colorBlending = vk::PipelineColorBlendStateCreateInfo {
		.logicOp         = vk::LogicOp::eCopy,
		.attachmentCount = uint32_t(colorBlendAttachments.size()),
		.pAttachments    = colorBlendAttachments.data()
	};
	
	auto depthStencilState = vk::PipelineDepthStencilStateCreateInfo {
		.depthTestEnable       = true,
		.depthWriteEnable      = true,
		.depthCompareOp        = vk::CompareOp::eLess,
		.depthBoundsTestEnable = false,
		.maxDepthBounds        = 1.0f
	};

	auto pipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo {
		.colorAttachmentCount    = uint32_t(format.attachmentFormats.colorFormats.size()),
		.pColorAttachmentFormats = format.attachmentFormats.colorFormats.data(),
		.depthAttachmentFormat   = format.attachmentFormats.depthFormat ? *format.attachmentFormats.depthFormat : vk::Format::eUndefined
	};

	auto pipelineStages = format.stages | std::views::transform([](const PipelineStage& s) {
		return vk::PipelineShaderStageCreateInfo {
			.stage  = s.stage,
			.module = s.module,
			.pName  = s.entrypoint
		};
	}) | std::ranges::to<std::vector>();

	auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo {
			.vertexBindingDescriptionCount   = uint32_t(format.vertexFormat.bindings.size()),
			.pVertexBindingDescriptions      = format.vertexFormat.bindings.data(),
			.vertexAttributeDescriptionCount = uint32_t(format.vertexFormat.attributes.size()),
			.pVertexAttributeDescriptions    = format.vertexFormat.attributes.data()
	};

	m_descriptorSetLayouts = format.descriptorSetLayouts | std::views::transform([&device](const std::optional<vk::DescriptorSetLayoutCreateInfo>& info) -> std::optional<vk::raii::DescriptorSetLayout> {
		if (info)
			return device.GetDevice().createDescriptorSetLayout(*info);

		return std::nullopt;
	}) | std::ranges::to<std::vector>();

	auto rawSetLayouts = m_descriptorSetLayouts | std::views::transform([](const std::optional<vk::raii::DescriptorSetLayout>& layout) {
		if (layout)
			return **layout;

		return vk::DescriptorSetLayout(nullptr);
	}) | std::ranges::to<std::vector>();

	auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo {
		.setLayoutCount         = static_cast<uint32_t>(rawSetLayouts.size()),
		.pSetLayouts            = rawSetLayouts.data(),
		.pushConstantRangeCount = format.pushConstantRange ? 1u : 0u,
		.pPushConstantRanges    = format.pushConstantRange ? &*format.pushConstantRange : nullptr
	};
	m_pipelineLayout = vk::raii::PipelineLayout(device.GetDevice(), pipelineLayoutInfo);

	auto pipelineInfo = vk::GraphicsPipelineCreateInfo {
		.pNext      = &pipelineRenderingCreateInfo,
		.stageCount = uint32_t(pipelineStages.size()),
		.pStages    = pipelineStages.data(),

		.pVertexInputState   = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState      = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState   = &multisampling,
		.pDepthStencilState  = format.attachmentFormats.depthFormat == vk::Format::eUndefined ? nullptr : &depthStencilState,
		.pColorBlendState    = &colorBlending,
		.pDynamicState       = &dynamicStateCreateInfo,
		.layout              = m_pipelineLayout,
		.renderPass          = nullptr
	};

	m_pipeline  = vk::raii::Pipeline(device.GetDevice(), nullptr, pipelineInfo);
}

GraphicsPipeline::~GraphicsPipeline()
{
	m_descriptorSetLayouts.clear();
	m_pipeline.clear();
	m_pipelineLayout.clear();
}

vk::Pipeline GraphicsPipeline::GetPipeline() const
{
	return *m_pipeline;
}

vk::PipelineLayout GraphicsPipeline::GetLayout() const
{
	return *m_pipelineLayout;
}

vk::raii::DescriptorSet GraphicsPipeline::CreateDescriptorSet(Device& device, uint32_t set)
{
	auto allocInfo = vk::DescriptorSetAllocateInfo {
		.descriptorPool     = *device.GetDescriptorPool(),
		.descriptorSetCount = 1,
		.pSetLayouts        = &**m_descriptorSetLayouts[set]
	};

	return std::move(device.GetDevice().allocateDescriptorSets(allocInfo)[0]);
}
