#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

using VertexAttributeDescriptions = std::vector<VkVertexInputAttributeDescription>;

template <typename V>
concept Vertex = requires(V v) {
    { v.GetAttributeDescriptions() } -> std::same_as<VertexAttributeDescriptions>;
};

template <Vertex V>
VkVertexInputBindingDescription GetBindingDescription(uint32_t binding,
                                                      VkVertexInputRate inputRate)
{
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = binding;
    bindingDescription.stride = sizeof(V);
    bindingDescription.inputRate = inputRate;
    return bindingDescription;
}

struct ColoredVertex {
    glm::vec3 Position;
    glm::vec3 Color;

    static VertexAttributeDescriptions GetAttributeDescriptions()
    {
        const std::vector<VkVertexInputAttributeDescription> VertexAttributeDescriptions{
            // location, binding, format, offset
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ColoredVertex, Position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ColoredVertex, Color)},
        };

        return VertexAttributeDescriptions;
    }
};