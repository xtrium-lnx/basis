#pragma once

#include <basis/device.h>
#include <basis/shadermodule.h>

#include <glm/glm.hpp>
#include <string>

namespace basis
{
	void Transition(vk::raii::CommandBuffer& cb, vk::ImageLayout from, vk::ImageLayout to, const vk::Image& image, bool isDepth = false, bool force = false);
	vk::ImageLayout ClearColor(vk::raii::CommandBuffer& cb, const vk::Image& image, vk::ImageLayout previousLayout, const glm::vec4& color);

	template<typename T>
	void BindToDescriptorSet(Device& device, const vk::raii::DescriptorSet& ds, uint32_t binding, const T& target);
}
