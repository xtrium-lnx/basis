#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vk::raii { class CommandBuffer; }

namespace basis
{
	class Buffer;
	class Device;

	class Mesh
	{
	public:
		struct Primitive
		{
			std::string      name;
			uint32_t         startIndex;
			uint32_t         indexCount;
			bool             enabled = true;
		};

		static constexpr size_t kMAX_BUFFERS = 4; // For now : position, normal, UV, tangents

		std::array<std::unique_ptr<Buffer>, kMAX_BUFFERS> vertexBuffers = { nullptr };
		std::unique_ptr<Buffer>                           indexBuffer   = nullptr;
		uint32_t                                          indexCount    = 0;
		std::vector<Primitive>                            primitives    = {};

		Mesh() = default;
		~Mesh();

		void Bind(const vk::raii::CommandBuffer& cb) const;

		static std::unique_ptr<Mesh> FromObj(Device& device, const std::string& data);
	};
}
