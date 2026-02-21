#include <basis/mesh.h>

#include <basis/device.h>
#include <basis/buffer.h>

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include <rapidobj.hpp>

#include <sstream>
#include <print>
#include <ranges>
#include <streambuf>

using namespace basis;

Mesh::Mesh(Device& device)
    : owner(device)
{
}

Mesh::~Mesh()
{
    for (auto& vb : vertexBuffers)
        owner.Defer([buffer = std::move(vb)] {});

    owner.Defer([buffer = std::move(indexBuffer)] {});
}

void Mesh::Bind(const vk::raii::CommandBuffer& cb) const
{
	auto vb = vertexBuffers
		| std::views::transform([](const std::unique_ptr<Buffer>& b) { return b->GetBuffer(); })
		| std::ranges::to<std::vector>();

	auto vbOffsets = vertexBuffers
		| std::views::transform([](const std::unique_ptr<Buffer>&) { return vk::DeviceSize(0); })
		| std::ranges::to<std::vector>();

	cb.bindVertexBuffers(0, vb, vbOffsets);
	cb.bindIndexBuffer(indexBuffer->GetBuffer(), 0, vk::IndexType::eUint32);
}

// ----------------------------------------------------------------------------

struct IndexKey
{
    int position_index;
    int normal_index;
    int texcoord_index;

    bool operator==(const IndexKey& other) const
    {
        return position_index == other.position_index
            && normal_index == other.normal_index
            && texcoord_index == other.texcoord_index;
    }
};

struct IndexKeyHash
{
    size_t operator()(const IndexKey& key) const
    {
        return (size_t)key.position_index ^ ((size_t)key.normal_index << 10) ^ ((size_t)key.texcoord_index << 20);
    }
};

std::vector<glm::vec3> s_ComputeTangents(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec2>& texcoords,
    const std::vector<uint32_t>& indices
) {
    std::vector<glm::vec3> tangents(normals.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        const glm::vec3& p0 = positions[i0];
        const glm::vec3& p1 = positions[i1];
        const glm::vec3& p2 = positions[i2];
        const glm::vec2& uv0 = texcoords[i0];
        const glm::vec2& uv1 = texcoords[i1];
        const glm::vec2& uv2 = texcoords[i2];

        glm::vec3 edge1 = p1 - p0;
        glm::vec3 edge2 = p2 - p0;
        glm::vec2 deltaUV1 = uv1 - uv0;
        glm::vec2 deltaUV2 = uv2 - uv0;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        if (std::isfinite(f))
        {
            glm::vec3 tangent;
            tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

            tangents[i0] += tangent;
            tangents[i1] += tangent;
            tangents[i2] += tangent;
        }

        for (size_t i = 0; i < tangents.size(); ++i)
        {
            const glm::vec3& n = normals[i];
            glm::vec3& t = tangents[i];

            // Gram-Schmidt orthogonalization
            t = glm::normalize(t - n * glm::dot(n, t));

            if (glm::length2(t) < 0.001f)
            {
                if (glm::abs(n.x) < 0.9f)
                    t = glm::normalize(glm::cross(n, glm::vec3(1.0f, 0.0f, 0.0f)));
                else
                    t = glm::normalize(glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f)));
            }
        }
    }

    return tangents;
}

std::unique_ptr<Mesh> Mesh::FromObj(Device& device, const std::string& data)
{
    auto result = std::make_unique<Mesh>(device);
    std::istringstream is(data);

    auto objData = rapidobj::ParseStream(is);
    if (!rapidobj::Triangulate(objData))
        std::println("Unable to triangulate mesh data, expect rendering errors");

    std::vector<uint32_t>                                indices;
    std::unordered_map<IndexKey, uint32_t, IndexKeyHash> indexHashmap;

    for (const auto& shape : objData.shapes)
    {
        if (shape.mesh.indices.empty())
            continue;

        size_t firstIndex = indices.size();

        for (const auto& index : shape.mesh.indices)
        {
            IndexKey key{ index.position_index, index.normal_index, index.texcoord_index };
            if (!indexHashmap.contains(key))
                indexHashmap[key] = uint32_t(indexHashmap.size());

            indices.push_back(indexHashmap[key]);
        }

        result->primitives.push_back(Mesh::Primitive {
            .name = shape.name,
            .startIndex = uint32_t(firstIndex),
            .indexCount = uint32_t(indices.size() - firstIndex)
        });
    }

    result->indexBuffer = std::make_unique<Buffer>(device, vk::BufferUsageFlagBits::eIndexBuffer, indices);
    result->indexCount  = uint32_t(indices.size());

    std::vector<glm::vec3> positions(indexHashmap.size());
    for (auto& [k, index] : indexHashmap)
        positions[index] = glm::vec3(
            objData.attributes.positions[k.position_index * 3 + 0],
            objData.attributes.positions[k.position_index * 3 + 1],
            objData.attributes.positions[k.position_index * 3 + 2]
        );

    result->vertexBuffers[0] = std::make_unique<Buffer>(device, vk::BufferUsageFlagBits::eVertexBuffer, positions);

    std::vector<glm::vec3> normals;
    if (!objData.attributes.normals.empty())
    {
        normals.resize(indexHashmap.size(), glm::vec3(0.0f));
        for (auto& [k, index] : indexHashmap)
            normals[index] = glm::vec3(
                objData.attributes.normals[k.normal_index * 3 + 0],
                objData.attributes.normals[k.normal_index * 3 + 1],
                objData.attributes.normals[k.normal_index * 3 + 2]
            );

        result->vertexBuffers[1] = std::make_unique<Buffer>(device, vk::BufferUsageFlagBits::eVertexBuffer, normals);
    }

    std::vector<glm::vec2> uvs;
    if (!objData.attributes.texcoords.empty())
    {
        uvs.resize(indexHashmap.size(), glm::vec2(0.0f));
        for (auto& [k, index] : indexHashmap)
            uvs[index] = glm::vec2(
                objData.attributes.texcoords[k.texcoord_index * 2 + 0],
                objData.attributes.texcoords[k.texcoord_index * 2 + 1]
            );

        result->vertexBuffers[2] = std::make_unique<Buffer>(device, vk::BufferUsageFlagBits::eVertexBuffer, uvs);
    }

    if (!normals.empty() && !uvs.empty())
    {
        std::vector<glm::vec3> tangents = s_ComputeTangents(positions, normals, uvs, indices);
        result->vertexBuffers[3] = std::make_unique<Buffer>(device, vk::BufferUsageFlagBits::eVertexBuffer, tangents);
    }

    std::println(" -> {} vertices, {} triangles", positions.size(), indices.size() / 3);
    return std::move(result);
}
