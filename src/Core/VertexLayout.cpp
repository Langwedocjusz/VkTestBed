#include "VertexLayout.h"

#include <utility>

static VkFormat GetFormat(Vertex::AttributeType type)
{
    using enum Vertex::AttributeType;

    switch (type)
    {
    case Float:
        return VK_FORMAT_R32_SFLOAT;
    case Vec2:
        return VK_FORMAT_R32G32_SFLOAT;
    case Vec3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case Vec4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }

    std::unreachable();
}

static uint32_t GetSize(Vertex::AttributeType type)
{
    using enum Vertex::AttributeType;

    switch (type)
    {
    case Float:
        return 4;
    case Vec2:
        return 8;
    case Vec3:
        return 12;
    case Vec4:
        return 16;
    }

    std::unreachable();
}

static uint32_t GetSize(const Vertex::Layout &layout)
{
    uint32_t res = 0;

    for (auto type : layout)
        res += GetSize(type);

    return res;
}

Vertex::AttributeDescriptions Vertex::GetAttributeDescriptions(const Layout &layout)
{
    AttributeDescriptions res;

    uint32_t currentOffset = 0;

    for (auto attributeType : layout)
    {
        res.push_back({static_cast<uint32_t>(res.size()), 0, GetFormat(attributeType),
                       currentOffset});

        currentOffset += GetSize(attributeType);
    }

    return res;
}

Vertex::BindingDescription Vertex::GetBindingDescription(const Layout &layout,
                                                         uint32_t binding,
                                                         VkVertexInputRate inputRate)
{
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = binding;
    bindingDescription.stride = GetSize(layout);
    bindingDescription.inputRate = inputRate;
    return bindingDescription;
}