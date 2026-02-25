#include "VertexLayout.h"
#include "Pch.h"

bool operator==(const Vertex::PushLayout &lhs, const Vertex::PushLayout &rhs)
{
    bool res = true;

    res = res && (lhs.HasTexCoord == rhs.HasTexCoord);
    res = res && (lhs.HasNormal == rhs.HasNormal);
    res = res && (lhs.HasTangent == rhs.HasTangent);
    res = res && (lhs.HasColor == rhs.HasColor);

    return res;
}

//TODO: This code shouldn't be necessary. Equivalent code should be generated
//by variant itself, but somehow that doesn't work for now.
bool operator==(const Vertex::Layout &lhs, const Vertex::Layout &rhs)
{
    const bool lpush = std::holds_alternative<Vertex::PushLayout>(lhs);
    const bool rpush = std::holds_alternative<Vertex::PushLayout>(rhs);

    const bool lpull = !lpush;
    const bool rpull = !rpush;

    if (lpush && rpush)
    {
        return std::get<Vertex::PushLayout>(lhs) == std::get<Vertex::PushLayout>(rhs);
    }
    else if (lpull && rpull)
    {
        return std::get<Vertex::PullLayout>(lhs) == std::get<Vertex::PullLayout>(rhs);
    }
    
    return false;
}

uint32_t Vertex::GetSize(const Vertex::PushLayout &layout)
{
    uint32_t res = 3 * sizeof(float);

    if (layout.HasTexCoord)
        res += 2 * sizeof(float);

    if (layout.HasNormal)
        res += 3 * sizeof(float);

    if (layout.HasTangent)
        res += 4 * sizeof(float);

    if (layout.HasColor)
        res += 4 * sizeof(float);

    return res;
}

Vertex::AttributeDescriptions Vertex::GetAttributeDescriptions(const PushLayout &layout)
{
    AttributeDescriptions res;
    uint32_t              offset = 0;

    auto NewDescription = [&res, &offset](VkFormat format) {
        return VkVertexInputAttributeDescription{
            .location = static_cast<uint32_t>(res.size()),
            .binding  = 0,
            .format   = format,
            .offset   = offset,
        };
    };

    // Add position:
    res.push_back(NewDescription(VK_FORMAT_R32G32B32_SFLOAT));
    offset += 3 * sizeof(float);

    // Conditionally add other attributes:
    if (layout.HasTexCoord)
    {
        res.push_back(NewDescription(VK_FORMAT_R32G32_SFLOAT));
        offset += 2 * sizeof(float);
    }

    if (layout.HasNormal)
    {
        res.push_back(NewDescription(VK_FORMAT_R32G32B32_SFLOAT));
        offset += 3 * sizeof(float);
    }

    if (layout.HasTangent)
    {
        res.push_back(NewDescription(VK_FORMAT_R32G32B32A32_SFLOAT));
        offset += 4 * sizeof(float);
    }

    if (layout.HasColor)
    {
        res.push_back(NewDescription(VK_FORMAT_R32G32B32A32_SFLOAT));
        offset += 4 * sizeof(float);
    }

    return res;
}

Vertex::BindingDescription Vertex::GetBindingDescription(const PushLayout &layout,
                                                         uint32_t          binding,
                                                         VkVertexInputRate inputRate)
{
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = binding;
    bindingDescription.stride    = GetSize(layout);
    bindingDescription.inputRate = inputRate;
    return bindingDescription;
}