#pragma once

#include <variant>
#include <vector>

#include "volk.h"

namespace Vertex
{
// Struct defining supported vertex types to be used with push-based approach.
// Represents limitied subset of gltf-representable vertex layouts
// Implicitly includes position, so the underlying struct layout will be
// pos (vec3), texcoord (vec2), normal (vec3), tangent (vec4), color (vec3)
// with the possibility that fields are skipped.
struct PushLayout {
    bool HasTexCoord = false;
    bool HasNormal   = false;
    bool HasTangent  = false;
    bool HasColor    = false;
};

// Enum defining supported vertex types to be used with
// programmatic vertex pulling:
enum class PullLayout
{
    Naive,
    Compressed,
};

// Union type covering all supported vertex types:
using Layout = std::variant<PushLayout, PullLayout>;

uint32_t GetSize(const PushLayout &layout);

// Utils to quickly construct desctiptions for pipeline input when using push:
using AttributeDescriptions = std::vector<VkVertexInputAttributeDescription>;
using BindingDescription    = VkVertexInputBindingDescription;

AttributeDescriptions GetAttributeDescriptions(const PushLayout &layout);

BindingDescription GetBindingDescription(const PushLayout &layout, uint32_t binding,
                                         VkVertexInputRate inputRate);
} // namespace Vertex

bool operator==(const Vertex::PushLayout &lhs, const Vertex::PushLayout &rhs);
bool operator==(const Vertex::Layout &lhs, const Vertex::Layout &rhs);