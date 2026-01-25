#pragma once

#include "OpaqueBuffer.h"
#include "VertexLayout.h"

#include <glm/glm.hpp>

struct GeometryLayout {
    Vertex::Layout VertexLayout;
    VkIndexType IndexType;

    bool IsCompatible(const GeometryLayout &other);
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

/// Axis aligned bounding box
struct AABB {
    glm::vec3 Center;
    glm::vec3 Extent;

    [[nodiscard]] bool IsInView(glm::mat4 mvp) const;
    [[nodiscard]] AABB GetConservativeTransformedAABB(glm::mat4 transform) const;

    [[nodiscard]] AABB MaxWith(AABB other) const;

    [[nodiscard]] std::array<glm::vec3, 8> GetCorners() const;
};

struct GeometryData {
    GeometryData() = default;
    GeometryData(const GeometrySpec &spec);

    GeometryLayout Layout;
    OpaqueBuffer VertexData;
    OpaqueBuffer IndexData;
    AABB BBox;
};