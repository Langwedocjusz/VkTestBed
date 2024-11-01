#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace Vertex
{

enum class AttributeType
{
    Float,
    Vec2,
    Vec3,
    Vec4
};

using Layout = std::vector<AttributeType>;

using AttributeDescriptions = std::vector<VkVertexInputAttributeDescription>;
using BindingDescription = VkVertexInputBindingDescription;

AttributeDescriptions GetAttributeDescriptions(const Layout &layout);

BindingDescription GetBindingDescription(const Layout &layout, uint32_t binding,
                                         VkVertexInputRate inputRate);
} // namespace Vertex