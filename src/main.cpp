#define TS_VFS_IMPLEMENTATION
#include <ts/ts_vfs.h>

#include <ts/ts_ecs.h>

//#define TS_VFS_ZIP_IMPLEMENTATION
//#include <ts/ts_vfs_zip.h>

#include <basis/window.h>
#include <basis/device.h>
#include <basis/image.h>
#include <basis/helpers.h>
#include <basis/graphicspipeline.h>
#include <basis/buffer.h>
#include <basis/mesh.h>

#include <memory>
#include <print>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct CameraData
{
	glm::mat4 viewMatrix;
	glm::mat4 viewInverseMatrix;
	glm::mat4 projectionMatrix;
};

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

struct Transform
{
	glm::vec3 position    = glm::vec3(0.0f);
	glm::quat orientation = glm::identity<glm::quat>();
	glm::vec3 scale       = glm::vec3(1.0f);

	glm::mat4 ComputeMatrix() const
	{
		return glm::translate(glm::mat4(1.0f), position)
			 * glm::mat4_cast(orientation)
			 * glm::scale(glm::mat4(1.0f), scale);
	}
};

struct MeshComponent
{
	std::unique_ptr<basis::Mesh> mesh;
};

struct CameraComponent
{
	glm::vec3 target      = glm::vec3(0.0f);
	glm::vec3 up          = glm::vec3(0.0f, 1.0f, 0.0f);

	float     fov         = 45.0f;
	glm::vec2 depthBounds = glm::vec2(0.1f, 100.0f);

	glm::mat4 ComputeViewMatrix(const Transform& transform) const
	{
		return glm::lookAt(transform.position, target, up);
	}

	glm::mat4 ComputeProjectionMatrix(float aspectRatio) const
	{
		return glm::perspective(glm::radians(fov), aspectRatio, depthBounds.x, depthBounds.y);
	}
};

int main(int argc, char** argv)
{
	auto window = std::make_unique<basis::Window>(basis::WindowDesc { { 1600, 900 }, "Basis example" });
	auto device = std::make_unique<basis::Device>(*window);
	auto vfs    = std::make_unique<ts::Vfs>();

	ts::Scene scene;
	auto monkey = scene.Spawn(
		Transform {},
		MeshComponent {
			.mesh = vfs->Open("/meshes/suzanne.obj")
				.and_then([](ts::VfsFile f) { return f.ReadText(); })
				.transform([&device](const std::string& text) { return basis::Mesh::FromObj(*device, text); })
				.value()
		}
	);

	auto camera = scene.Spawn(
		Transform{},
		CameraComponent{}
	);

	auto pipeline = CreateBasicPipeline(*device, *vfs);

	auto cameraBuffer        = std::make_unique<basis::Buffer>(*device, vk::BufferUsageFlagBits::eUniformBuffer, sizeof(CameraData));
	auto cameraDescriptorSet = pipeline->CreateDescriptorSet(*device, 0);
	basis::BindToDescriptorSet(*device, cameraDescriptorSet, 0, *cameraBuffer);

	auto depthBuffer = std::make_unique<basis::Image>(*device, basis::ImageDesc {
		.type   = vk::ImageType::e2D,
		.format = vk::Format::eD32Sfloat,
		.width  = window->Size().x,
		.height = window->Size().y,
		.usage  = vk::ImageUsageFlagBits::eDepthStencilAttachment
	});
	auto depthBufferCurrentLayout = vk::ImageLayout::eUndefined;

	float t = 0.0f;

	do
	{
		window->PollEvents();
		auto [image, imageAvailable, renderFinished] = device->AcquireNextFrame();

		scene.RequireComponent<Transform>(camera).position = glm::vec3(
			6.0f * glm::cos(t),
			2.0f,
			6.0f * glm::sin(t)
		);
		
		/**
		 * Updating the view matrix, aka. the camera's position and orientation in the world.
		 * Note that this time, we're using Buffer::UploadAsync. This is because this one is non-blocking.
		 * Inside, it sets up a persistent staging buffer (the one in the sync Upload is temporary),
		 * copies the data to it, and issues a copy command.
		 * But instead of waiting, it returns a semaphore, which we wait on only when we actually need the data.
		 */
		CameraData cameraData;
		scene.Query<Transform, CameraComponent>().Single([&](ts::Entity, Transform& transform, CameraComponent& camera) {
			cameraData.viewMatrix        = camera.ComputeViewMatrix(transform);
			cameraData.viewInverseMatrix = glm::inverse(cameraData.viewMatrix);
			cameraData.projectionMatrix  = camera.ComputeProjectionMatrix(float(image.Size().x) / float(image.Size().y));
		});
		auto cameraUploadComplete = cameraBuffer->UploadAsync(*device, cameraData);

		auto cb = device->AcquireCommandBuffer();
		cb->begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

		basis::Transition(*cb, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, image.GetImage());

		if (depthBufferCurrentLayout != vk::ImageLayout::eDepthAttachmentOptimal)
		{
			basis::Transition(*cb, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal, depthBuffer->GetImage(), true);
			depthBufferCurrentLayout = vk::ImageLayout::eDepthAttachmentOptimal;
		}

		auto colorAttachmentInfo = vk::RenderingAttachmentInfo {
			.imageView   = image.GetView(),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp      = vk::AttachmentLoadOp::eClear,
			.storeOp     = vk::AttachmentStoreOp::eStore,
			.clearValue  = vk::ClearColorValue(0.1f, 0.2f, 0.3f, 1.0f)
		};

		auto depthAttachmentInfo = vk::RenderingAttachmentInfo {
			.imageView   = depthBuffer->GetView(),
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp      = vk::AttachmentLoadOp::eClear,
			.storeOp     = vk::AttachmentStoreOp::eStore,
			.clearValue  = vk::ClearDepthStencilValue(1.0f)
		};

		cb->beginRendering(vk::RenderingInfo {
			.renderArea = {
				.offset = { 0, 0 },
				.extent = { window->Size().x, window->Size().y }
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
		cb->setViewport(0, vk::Viewport { 0.0f, float(window->Size().y), float(window->Size().x), -float(window->Size().y), 0.0f, 1.0f });
		cb->setScissor(0, { vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(window->Size().x, window->Size().y)) });

		cb->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->GetPipeline());

		// Bindings are done in order, so vertexBuffer is binding 0, colorBuffer is binding 1
		cb->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->GetLayout(), 0, { cameraDescriptorSet }, {});

		scene.Query<Transform, MeshComponent>().Each([&](ts::Entity, Transform& transform, MeshComponent& meshComponent) {
			meshComponent.mesh->Bind(*cb);
			auto model = transform.ComputeMatrix();
			cb->pushConstants(
				pipeline->GetLayout(),
				vk::ShaderStageFlagBits::eVertex,
				0,
				vk::ArrayProxy<const std::byte>(uint32_t(sizeof(glm::mat4)), reinterpret_cast<const std::byte*>(&model[0][0]))
			);
			cb->drawIndexed(meshComponent.mesh->indexCount, 1, 0, 0, 0);
		});

		cb->endRendering();
		
		basis::Transition(*cb, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, image.GetImage());
		cb->end();

		/**
		 * Here, we're waiting on two semaphores :
		 * - imageAvailable tells us the presentation for the current image is done so it's ours to use
		 * - cameraUploadComplete tells us the (async) upload of the camera buffer is done, so we can use it in the shader
		 */
		device->Submit(cb, { imageAvailable, cameraUploadComplete }, renderFinished);
		device->ReleaseCommandBuffer(cb);

		// if (t > 1.0f)
		// 	scene.Kill(monkey);

		device->Present({ renderFinished });

		// BAD PRACTICE, ONLY HERE FOR DEMONSTRATION PURPOSES
		// Use delta-time from a clock (ie. std::chrono) instead.
		t += 1.0f / 60.0f;
	} while (!window->ShouldClose());

	device->WaitForIdle();

	scene.Clear();
	depthBuffer.reset();

	cameraDescriptorSet.clear();
	cameraBuffer.reset();
	pipeline.reset();

	vfs.reset();

	device->FlushDeferred();
	device.reset();
	window.reset();
	return 0;
}
