#pragma once

#include <vector>

#include "volk.h"

namespace Vertex
{
// Represents limitied subset of gltf-representable vertex layouts
// Implicitly includes position, so the underlying struct layout will be
// pos (vec3), texcoord (vec2), normal (vec3), tangent (vec4), color (vec3)
// with the possibility that fields are skipped.

struct Layout {
    bool HasTexCoord = false;
    bool HasNormal   = false;
    bool HasTangent  = false;
    bool HasColor    = false;

    bool operator==(const Layout &other);
};

// Utils to quickly construct vertex desctiptions for pipeline input:

using AttributeDescriptions = std::vector<VkVertexInputAttributeDescription>;
using BindingDescription    = VkVertexInputBindingDescription;

uint32_t GetSize(const Layout &layout);

AttributeDescriptions GetAttributeDescriptions(const Layout &layout);

BindingDescription GetBindingDescription(const Layout &layout, uint32_t binding,
                                         VkVertexInputRate inputRate);
} // namespace Vertex