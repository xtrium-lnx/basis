#include <basis/basicrenderer.h>

#include <basis/window.h>
#include <basis/device.h>
#include <basis/buffer.h>
#include <basis/image.h>
#include <basis/graphicspipeline.h>
#include <basis/shadermodule.h>
#include <basis/helpers.h>
#include <basis/mesh.h>

#include <ts/ts_vfs.h>

using namespace basis;

std::unique_ptr<basis::GraphicsPipeline> CreateBasicPipeline(basis::Device& device, ts::Vfs& vfs)
{
	auto uniformBufferBinding = vk::DescriptorSetLayoutBinding {
		.binding         = 0,
		.descriptorType  = vk::DescriptorType::eUniformBuffer,
		.descriptorCount = 1,
		.stageFlags      = vk::ShaderStageFlagBits::eVertex
	};

	auto basicModule = vfs.Open("/shaders/basic.slang.spv")
		.and_then([](ts::VfsFile f) { return f.ReadBytes(); })
		.transform([&device](const std::vector<std::byte>& bytes) { return basis::ShaderModule(device, bytes.data(), bytes.size()); })
		.value();

	return std::make_unique<basis::GraphicsPipeline>(
		device,
		basis::PipelineFormat {
			.stages = {
				{ basicModule.GetModule(), vk::ShaderStageFlagBits::eVertex,   "vs_main" },
				{ basicModule.GetModule(), vk::ShaderStageFlagBits::eFragment, "fs_main" }
			},
			.vertexFormat = {
				.bindings = { // What buffers has what stride and goes where (cf. vkCmdBindVertexBuffers)
					{ 0, sizeof(glm::vec3), vk::VertexInputRate::eVertex },
					{ 1, sizeof(glm::vec3), vk::VertexInputRate::eVertex },
					{ 2, sizeof(glm::vec2), vk::VertexInputRate::eVertex },
					{ 3, sizeof(glm::vec3), vk::VertexInputRate::eVertex }
				},
				.attributes = { // Which attribute of the vertex data uses which binding (and its format) (corresponds to what's in the shader)
					{ 0, 0, vk::Format::eR32G32B32Sfloat, 0 }, // Attribute 0 is using binding 0. It's in R32G32B32 (float3 position)
					{ 1, 1, vk::Format::eR32G32B32Sfloat, 0 }, // Attribute 1 is using binding 1. It's in R32G32B32 (float3 normal)
					{ 2, 2, vk::Format::eR32G32Sfloat,    0 }, // Attribute 2 is using binding 2. It's in R32G32    (float2 uv)
					{ 3, 3, vk::Format::eR32G32B32Sfloat, 0 }  // Attribute 3 is using binding 3. It's in R32G32B32 (float3 tangent)
				}
			},
			.attachmentFormats = {
				.colorFormats = { // Color attachments (only one for us)
					vk::Format::eB8G8R8A8Srgb
				},
				.depthFormat = vk::Format::eD32Sfloat
			},
			.descriptorSetLayouts = {
				vk::DescriptorSetLayoutCreateInfo {
					.bindingCount = 1,
					.pBindings    = &uniformBufferBinding
				}
			},
			.pushConstantRange = vk::PushConstantRange {
				vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4)
			}
		}
	);

	// Note: basicModule will be destroyed here.
	// Just like when compiling an exe you can delete the temp files once the exe is done,
	// you can destroy the ShaderModule once the pipeline is created.
}

BasicRenderer::BasicRenderer(Window& window, Device& device, ts::Vfs& vfs)
	: m_device(device)
	, m_cameraData({})
{
	m_pipeline = CreateBasicPipeline(device, vfs);

	m_cameraBuffer        = std::make_unique<basis::Buffer>(device, vk::BufferUsageFlagBits::eUniformBuffer, sizeof(CameraData));
	m_cameraDescriptorSet = m_pipeline->CreateDescriptorSet(device, 0);
	basis::BindToDescriptorSet(device, m_cameraDescriptorSet, 0, *m_cameraBuffer);

	m_depthBuffer = std::make_unique<basis::Image>(device, basis::ImageDesc {
		.type   = vk::ImageType::e2D,
		.format = vk::Format::eD32Sfloat,
		.width  = window.Size().x,
		.height = window.Size().y,
		.usage  = vk::ImageUsageFlagBits::eDepthStencilAttachment
	});
}

void BasicRenderer::SetCamera(const glm::mat4& view, const glm::mat4& projection)
{
	m_cameraData.viewMatrix        = view;
	m_cameraData.viewInverseMatrix = glm::inverse(view);
	m_cameraData.projectionMatrix  = projection;
}

void BasicRenderer::PushGeometry(const glm::mat4& model, Mesh* mesh)
{
	m_geometry.push_back({ model, mesh });
}

void BasicRenderer::Render(basis::Image& target, const std::vector<vk::Semaphore>& waitOps, vk::Semaphore signalOp)
{
	/**
	 * Updating the camera matrices.
	 * Note that this time, we're using Buffer::UploadAsync. This is because this one is non-blocking.
	 * Inside, it sets up a persistent staging buffer (the one in the sync Upload is temporary),
	 * copies the data to it, and issues a copy command.
	 * But instead of waiting, it returns a semaphore, which we wait on only when we actually need the data.
	 */
	auto cameraUploadComplete = m_cameraBuffer->UploadAsync(m_device, m_cameraData);

	auto cb = m_device.AcquireCommandBuffer();
	cb->begin(vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

	basis::Transition(*cb, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, target.GetImage());
	{
		static bool isDepthBufferInProperLayout = false;
		if (!isDepthBufferInProperLayout)
		{
			basis::Transition(*cb, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal, m_depthBuffer->GetImage(), true);
			isDepthBufferInProperLayout = true;
		}
	}

	auto colorAttachmentInfo = vk::RenderingAttachmentInfo {
		.imageView   = target.GetView(),
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp      = vk::AttachmentLoadOp::eClear,
		.storeOp     = vk::AttachmentStoreOp::eStore,
		.clearValue  = vk::ClearColorValue(0.1f, 0.2f, 0.3f, 1.0f)
	};

	auto depthAttachmentInfo = vk::RenderingAttachmentInfo {
		.imageView   = m_depthBuffer->GetView(),
		.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		.loadOp      = vk::AttachmentLoadOp::eClear,
		.storeOp     = vk::AttachmentStoreOp::eStore,
		.clearValue  = vk::ClearDepthStencilValue(1.0f)
	};

	cb->beginRendering(vk::RenderingInfo {
		.renderArea = {
			.offset = { 0, 0 },
			.extent = { target.Size().x, target.Size().y }
		},
		.layerCount           = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments    = &colorAttachmentInfo,
		.pDepthAttachment     = &depthAttachmentInfo
	});

	// Note : We're using dynamic states for viewport and scissor, which means they're not set directly in the pipeline
	//        so we have to manually set them up at draw time.
	//        You might also notice that we've done a bit of weird math on the viewport bounds. That is because, in Vulkan,
	//        the viewport is flipped over (origin is top-left) compared to OpenGL (where origin is bottom left, like in math and like I've shown in class).
	cb->setViewport(0, vk::Viewport { 0.0f, float(target.Size().y), float(target.Size().x), -float(target.Size().y), 0.0f, 1.0f });
	cb->setScissor(0, { vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(target.Size().x, target.Size().y)) });

	cb->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline->GetPipeline());
	cb->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline->GetLayout(), 0, { m_cameraDescriptorSet }, {});

	for (auto& [modelMatrix, mesh] : m_geometry)
	{
		mesh->Bind(*cb);
		cb->pushConstants<glm::mat4>(m_pipeline->GetLayout(), vk::ShaderStageFlagBits::eVertex, 0, { modelMatrix });
		cb->drawIndexed(mesh->indexCount, 1, 0, 0, 0);
	}

	cb->endRendering();

	basis::Transition(*cb, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, target.GetImage());
	cb->end();

	auto internalWaitOps = waitOps;
	internalWaitOps.push_back(cameraUploadComplete);
	m_device.Submit(cb, internalWaitOps, signalOp);
	m_device.ReleaseCommandBuffer(cb);

	m_geometry.clear();
}
