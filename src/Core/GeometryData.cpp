#include "GeometryData.h"
#include "Pch.h"
#include "VertexLayout.h"

#include <limits>

GeometryData::GeometryData(const GeometrySpec &spec)
    : VertexData(spec.VertCount, spec.VertBuffSize, spec.VertAlignment),
      IndexData(spec.IdxCount, spec.IdxBuffSize, spec.IdxAlignment)
{
}

bool operator==(const GeometryLayout &lhs, const GeometryLayout &rhs)
{
    const bool vertCompat = lhs.VertexLayout == rhs.VertexLayout;
    const bool idxCompat  = lhs.IndexType == rhs.IndexType;

    return idxCompat && vertCompat;
}

std::array<glm::vec3, 8> AABB::GetVertices() const
{
    return {
        // clang-format off
        Center + Extent * glm::vec3(-1, -1, -1), 
        Center + Extent * glm::vec3( 1, -1, -1),
        Center + Extent * glm::vec3( 1,  1, -1),  
        Center + Extent * glm::vec3(-1,  1, -1),
        Center + Extent * glm::vec3(-1, -1,  1), 
        Center + Extent * glm::vec3( 1, -1,  1),
        Center + Extent * glm::vec3( 1,  1,  1),  
        Center + Extent * glm::vec3(-1,  1,  1),
        // clang-format on
    };
}

std::array<std::array<size_t, 2>, 12> AABB::GetEdgesIds()
{
    return {
        {
         // clang-format off
        {0,1}, {1,2}, {2,3}, {3,0}, // Lower face
        {4,5}, {5,6}, {6,7}, {7,4}, // Upper face
        {0,4}, {1,5}, {2,6}, {3,7}  // Corners
                  // clang-format on
        }
    };
}

bool AABB::IsInView(glm::mat4 mvp) const
{
    auto corners = GetVertices();

    bool allFront = true;
    bool allBack  = true;
    bool allLeft  = true;
    bool allRight = true;
    bool allUp    = true;
    bool allDown  = true;

    for (auto corner : corners)
    {
        glm::vec4 homogeneous = mvp * glm::vec4(corner, 1.0f);

        allFront = allFront && (homogeneous.z > homogeneous.w);
        allBack  = allBack && (homogeneous.z < 0.0f);
        allLeft  = allLeft && (homogeneous.x < -homogeneous.w);
        allRight = allRight && (homogeneous.x > homogeneous.w);
        allUp    = allUp && (homogeneous.y > homogeneous.w);
        allDown  = allDown && (homogeneous.y < -homogeneous.w);
    }

    return !(allFront || allBack || allLeft || allRight || allUp || allDown);
}

AABB AABB::GetConservativeTransformedAABB(glm::mat4 transform) const
{
    auto corners = GetVertices();

    // Transform all corners:
    for (auto &corner : corners)
    {
        auto transformed = transform * glm::vec4(corner, 1.0f);
        corner           = glm::vec3(transformed);
    }

    // Get extremal coordinates of the transformed corners:
    auto vmin = glm::vec3(std::numeric_limits<float>::max());
    auto vmax = glm::vec3(std::numeric_limits<float>::lowest());

    for (auto corner : corners)
    {
        vmin = glm::min(vmin, corner);
        vmax = glm::max(vmax, corner);
    }

    // Repack into AABB:
    auto center = 0.5f * (vmax + vmin);
    auto extent = 0.5f * (vmax - vmin);

    return AABB{.Center = center, .Extent = extent};
}

[[nodiscard]] AABB AABB::MaxWith(AABB other) const
{
    glm::vec3 vmin = glm::min(Center - Extent, other.Center - other.Extent);
    glm::vec3 vmax = glm::max(Center + Extent, other.Center + other.Extent);

    auto center = 0.5f * (vmax + vmin);
    auto extent = 0.5f * (vmax - vmin);

    return AABB{.Center = center, .Extent = extent};
}
