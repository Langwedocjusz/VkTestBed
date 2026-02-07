#pragma once

#include <string>
#include <vector>

#include "volk.h"

namespace Vertex
{

enum class AttributeType
{
    Float,
    Vec2,
    Vec3,
    Vec4
};

std::string ToString(AttributeType type);

using Layout = std::vector<AttributeType>;

using AttributeDescriptions = std::vector<VkVertexInputAttributeDescription>;
using BindingDescription    = VkVertexInputBindingDescription;

uint32_t GetSize(const Vertex::Layout &layout);

AttributeDescriptions GetAttributeDescriptions(const Layout &layout);

BindingDescription GetBindingDescription(const Layout &layout, uint32_t binding,
                                         VkVertexInputRate inputRate);
} // namespace Vertex