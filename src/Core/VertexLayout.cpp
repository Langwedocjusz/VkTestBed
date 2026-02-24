#include "VertexLayout.h"
#include "Pch.h"

bool Vertex::Layout::operator==(const Layout &other)
{
    bool res = true;

    res = (HasTexCoord == other.HasTexCoord) && res;
    res = (HasNormal == other.HasNormal) && res;
    res = (HasTangent == other.HasTangent) && res;
    res = (HasColor == other.HasColor) && res;

    return res;
}

uint32_t Vertex::GetSize(const Vertex::Layout &layout)
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

Vertex::AttributeDescriptions Vertex::GetAttributeDescriptions(const Layout &layout)
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

Vertex::BindingDescription Vertex::GetBindingDescription(const Layout     &layout,
                                                         uint32_t          binding,
                                                         VkVertexInputRate inputRate)
{
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = binding;
    bindingDescription.stride    = GetSize(layout);
    bindingDescription.inputRate = inputRate;
    return bindingDescription;
}