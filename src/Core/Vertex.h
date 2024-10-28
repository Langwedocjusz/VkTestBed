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

//Vertex type naming convention:
//P - position
//C - color
//N - normal vector
//T - tangent vector
//B - bitangent vector
//X - tex coords

struct Vertex_PC {
    glm::vec3 Position;
    glm::vec3 Color;

    static VertexAttributeDescriptions GetAttributeDescriptions()
    {
        const std::vector<VkVertexInputAttributeDescription> VertexAttributeDescriptions{
            // location, binding, format, offset
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex_PC, Position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex_PC, Color)},
        };

        return VertexAttributeDescriptions;
    }
};

struct Vertex_PCN{
    glm::vec3 Position;
    glm::vec3 Color;
    glm::vec3 Normal;

    static VertexAttributeDescriptions GetAttributeDescriptions()
    {
        const std::vector<VkVertexInputAttributeDescription> VertexAttributeDescriptions{
            // location, binding, format, offset
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex_PCN, Position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex_PCN, Color)},
            {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex_PCN, Normal)},
        };

        return VertexAttributeDescriptions;
    }
};