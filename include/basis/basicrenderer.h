#pragma once

#include <memory>
#include <glm/glm.hpp>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace ts { class Vfs; }

namespace basis
{
	class Window;
	class Device;
	class Buffer;
	class GraphicsPipeline;
	class Image;
	class Mesh;
	
	class BasicRenderer
	{
		Device&                                  m_device;
		std::unique_ptr<basis::GraphicsPipeline> m_pipeline;

		struct CameraData
		{
			glm::mat4 viewMatrix;
			glm::mat4 viewInverseMatrix;
			glm::mat4 projectionMatrix;
		};
		CameraData                               m_cameraData;
		std::unique_ptr<basis::Buffer>           m_cameraBuffer;
		vk::raii::DescriptorSet                  m_cameraDescriptorSet = nullptr;

		std::unique_ptr<basis::Image>            m_depthBuffer;

		std::vector<std::pair<glm::mat4, Mesh*>> m_geometry;

	public:
		BasicRenderer(Window& window, Device& device, ts::Vfs& vfs);
		~BasicRenderer() = default;

		void SetCamera(const glm::mat4& view, const glm::mat4& projection);
		void PushGeometry(const glm::mat4& model, Mesh* mesh);

		void Render(basis::Image& target, const std::vector<vk::Semaphore>& waitOps, vk::Semaphore signalOp);
	};
}
