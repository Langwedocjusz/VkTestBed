#pragma once

#include "OpaqueBuffer.h"
#include "VertexLayout.h"

#include <glm/glm.hpp>

struct GeometryLayout {
    Vertex::Layout VertexLayout;
    VkIndexType IndexType;

    bool IsCompatible(const GeometryLayout &other)
    {
        bool idxCompat = IndexType == other.IndexType;
        // To-do: this will be weakened in the future:
        bool vertCompat = VertexLayout == other.VertexLayout;

        return idxCompat && vertCompat;
    }
};

struct GeometrySpec {
    size_t VertCount = 0;
    size_t VertBuffSize = 0;
    size_t VertAlignment = 4;
    size_t IdxCount = 0;
    size_t IdxBuffSize = 0;
    size_t IdxAlignment = 4;

    // Builds geometry spec based on vertex size and index type.
    // Assumes vertex alignment is equal to 4.
    template <typename IdxType>
    static constexpr GeometrySpec BuildS(size_t vertSize, size_t vertCount,
                                         size_t idxCount)
    {
        static_assert(std::is_same_v<IdxType, uint16_t> ||
                      std::is_same_v<IdxType, uint32_t>);

        constexpr auto idxAlignment = []() -> size_t {
            if constexpr (std::is_same_v<IdxType, uint16_t>)
                return 2;
            else
                return 4;
        }();

        return GeometrySpec{
            .VertCount = vertCount,
            .VertBuffSize = vertCount * vertSize,
            .VertAlignment = 4,
            .IdxCount = idxCount,
            .IdxBuffSize = idxCount * sizeof(IdxType),
            .IdxAlignment = idxAlignment,
        };
    }

    // Builds geometry spec based on vertex and index types.
    // Assumes vertex alignment is equal to 4.
    template <typename VertType, typename IdxType>
    static constexpr GeometrySpec BuildV(size_t vertCount, size_t idxCount)
    {
        return BuildS<IdxType>(sizeof(VertType), vertCount, idxCount);
    }
};

struct BoundingBox {
    glm::vec3 Center;
    glm::vec3 Extent;

    bool InView(glm::mat4 transform)
    {
        std::array<glm::vec3, 8> corners{
            Center + Extent * glm::vec3(-1, -1, -1),
            Center + Extent * glm::vec3(-1, -1, 1),
            Center + Extent * glm::vec3(-1, 1, -1),
            Center + Extent * glm::vec3(-1, 1, 1),
            Center + Extent * glm::vec3(1, -1, -1),
            Center + Extent * glm::vec3(1, -1, 1),
            Center + Extent * glm::vec3(1, 1, -1),
            Center + Extent * glm::vec3(1, 1, 1),
        };

        bool allFront = true;
        bool allBack = true;
        bool allLeft = true;
        bool allRight = true;
        bool allUp = true;
        bool allDown = true;

        for (auto corner : corners)
        {
            glm::vec4 homogeneous = transform * glm::vec4(corner, 1.0f);

            allFront = allFront && (homogeneous.z > homogeneous.w);
            allBack = allBack && (homogeneous.z < 0.0f);
            allLeft = allLeft && (homogeneous.x < -homogeneous.w);
            allRight = allRight && (homogeneous.x > homogeneous.w);
            allUp = allUp && (homogeneous.y > homogeneous.w);
            allDown = allDown && (homogeneous.y < -homogeneous.w);
        }

        return !(allFront || allBack || allLeft || allRight || allUp || allDown);
    }
};

struct GeometryData {
    GeometryData() = default;

    GeometryData(const GeometrySpec &spec)
        : VertexData(spec.VertCount, spec.VertBuffSize, spec.VertAlignment),
          IndexData(spec.IdxCount, spec.IdxBuffSize, spec.IdxAlignment)
    {
    }

    GeometryLayout Layout;
    OpaqueBuffer VertexData;
    OpaqueBuffer IndexData;
    BoundingBox BBox;
};