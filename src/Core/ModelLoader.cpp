#include "ModelLoader.h"
#include "Pch.h"

#include "GeometryData.h"
#include "TangentsGenerator.h"
#include "Vassert.h"
#include "VertexLayout.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <iostream>
#include <limits>

static fastgltf::Asset GetAsset(const std::filesystem::path &path,
                                bool                         loadBuffers = false)
{
    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_diffuse_transmission);
    auto             data = fastgltf::GltfDataBuffer::FromPath(path);

    vassert(data.error() == fastgltf::Error::None,
            "Failed to load a gltf file: " + path.string());

    auto loadOptions = fastgltf::Options::None;

    if (loadBuffers)
        loadOptions = fastgltf::Options::LoadExternalBuffers;

    auto load = parser.loadGltf(data.get(), path.parent_path(), loadOptions);

    vassert(load.error() == fastgltf::Error::None,
            "Failed to load a gltf file: " + path.string());

    return std::move(load.get());
}

fastgltf::Asset ModelLoader::GetGltf(const std::filesystem::path &filepath)
{
    return GetAsset(filepath, false);
}

fastgltf::Asset ModelLoader::GetGltfWithBuffers(const std::filesystem::path &filepath)
{
    return GetAsset(filepath, true);
}

static fastgltf::Accessor &GetAttributeAccessor(fastgltf::Asset     &gltf,
                                                fastgltf::Primitive &primitive,
                                                std::string_view     name)
{
    return gltf.accessors[primitive.findAttribute(name)->accessorIndex];
}

static size_t GetIndexCount(fastgltf::Asset &gltf, fastgltf::Primitive &primitive)
{
    auto &indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
    return indexAccessor.count;
}

static size_t GetVertexCount(fastgltf::Asset &gltf, fastgltf::Primitive &primitive)
{
    fastgltf::Accessor &posAccessor = GetAttributeAccessor(gltf, primitive, "POSITION");
    return posAccessor.count;
}

static tangen::VertexLayout GetLayout(const ModelConfig &config)
{
    uint32_t stride        = 3;
    uint32_t offsetTex     = 3;
    uint32_t offsetNormal  = 3;
    uint32_t offsetTangent = 3;

    if (config.LoadTexCoord)
    {
        stride += 2;
        offsetNormal += 2;
        offsetTangent += 2;
    }

    if (config.LoadNormals)
    {
        stride += 3;
        offsetTangent += 3;
    }

    if (config.LoadTangents)
    {
        stride += 4;
    }

    // TODO: support loading colors...

    return tangen::VertexLayout{
        .Stride         = stride,
        .OffsetPos      = {0, 1, 2},
        .OffsetTexCoord = {offsetTex, offsetTex + 1},
        .OffsetNormal   = {offsetNormal, offsetNormal + 1, offsetNormal + 2},
        .OffsetTangent  = {offsetTangent, offsetTangent + 1, offsetTangent + 2,
                           offsetTangent + 3},
    };
}

static GeometryLayout ToGeoLayout(const ModelConfig &config)
{
    Vertex::Layout vertexLayout = Vertex::PushLayout{
        .HasTexCoord = config.LoadTexCoord,
        .HasNormal   = config.LoadNormals,
        .HasTangent  = config.LoadTangents,
        .HasColor    = config.LoadColor,
    };

    return GeometryLayout{
        .VertexLayout = vertexLayout,
        .IndexType    = VK_INDEX_TYPE_UINT32,
    };
}

struct VertParsingResult {
    AABB BBox;
    bool GenerateTangents = false;
};

static VertParsingResult RetrieveVertices(fastgltf::Asset     &gltf,
                                          fastgltf::Primitive &primitive,
                                          const ModelConfig &config, GeometryData &geo)
{
    VertParsingResult res{};

    auto layout = GetLayout(config);

    auto vertCount = GetVertexCount(gltf, primitive);
    auto compCount = vertCount * layout.Stride;

    auto data = new (geo.VertexData.Data) float[compCount];

    // Retrieve the positions and calculate bounding box in one go:
    fastgltf::Accessor &posAccessor = GetAttributeAccessor(gltf, primitive, "POSITION");

    auto minCoords = glm::vec3(std::numeric_limits<float>::max());
    auto maxCoords = glm::vec3(std::numeric_limits<float>::lowest());

    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                  [&](glm::vec3 v, size_t index) {
                                                      auto idx = layout.Stride * index;

                                                      data[idx + 0] = v.x;
                                                      data[idx + 1] = v.y;
                                                      data[idx + 2] = v.z;

                                                      minCoords = glm::min(minCoords, v);
                                                      maxCoords = glm::max(maxCoords, v);
                                                  });

    res.BBox.Center = 0.5f * (maxCoords + minCoords);
    res.BBox.Extent = 0.5f * (maxCoords - minCoords);

    // Retrieve texture coords
    if (config.LoadTexCoord)
    {
        auto texcoordIt = primitive.findAttribute("TEXCOORD_0");

        if (texcoordIt != primitive.attributes.end())
        {
            fastgltf::Accessor &texcoordAccessor =
                gltf.accessors[texcoordIt->accessorIndex];

            fastgltf::iterateAccessorWithIndex<glm::vec2>(
                gltf, texcoordAccessor, [&](glm::vec2 v, size_t index) {
                    auto baseIdx = layout.Stride * index;

                    data[baseIdx + layout.OffsetTexCoord[0]] = v.x;
                    data[baseIdx + layout.OffsetTexCoord[1]] = v.y;
                });
        }

        else
        {
            std::cerr << "Gltf file: " << config.Filepath.string()
                      << "primitive doesn't contain texture coordinates.\n";
        }
    }

    // Retrieve normals
    if (config.LoadNormals)
    {
        auto normalIt = primitive.findAttribute("NORMAL");

        if (normalIt != primitive.attributes.end())
        {
            fastgltf::Accessor &normalAccessor = gltf.accessors[normalIt->accessorIndex];

            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                gltf, normalAccessor, [&](glm::vec3 v, size_t index) {
                    auto baseIdx = layout.Stride * index;

                    data[baseIdx + layout.OffsetNormal[0]] = v.x;
                    data[baseIdx + layout.OffsetNormal[1]] = v.y;
                    data[baseIdx + layout.OffsetNormal[2]] = v.z;
                });
        }

        else
        {
            std::cerr << "Gltf file: " << config.Filepath.string()
                      << "primitive doesn't contain normals.\n";
        }
    }

    // Retrieve tangents if present, schedule usage of mikktspace otherwise
    if (config.LoadTangents)
    {
        auto tangentIt = primitive.findAttribute("TANGENT");

        if (tangentIt != primitive.attributes.end())
        {
            fastgltf::Accessor &tangentAccessor =
                gltf.accessors[tangentIt->accessorIndex];

            fastgltf::iterateAccessorWithIndex<glm::vec4>(
                gltf, tangentAccessor, [&](glm::vec4 v, size_t index) {
                    auto baseIdx = layout.Stride * index;

                    data[baseIdx + layout.OffsetTangent[0]] = v.x;
                    data[baseIdx + layout.OffsetTangent[1]] = v.y;
                    data[baseIdx + layout.OffsetTangent[2]] = v.z;
                    data[baseIdx + layout.OffsetTangent[3]] = v.w;
                });
        }
        else
        {
            res.GenerateTangents = true;
        }
    }

    // TODO: Support loading colors...

    return res;
}

static void RetrieveIndices(fastgltf::Asset &gltf, fastgltf::Primitive &primitive,
                            GeometryData &geo)
{
    auto &accessor = gltf.accessors[primitive.indicesAccessor.value()];

    auto   Indices = new (geo.IndexData.Data) uint32_t[accessor.count];
    size_t current = 0;

    fastgltf::iterateAccessor<std::uint32_t>(gltf, accessor, [&](std::uint32_t idx) {
        Indices[current] = idx;
        current++;
    });
}

GeometryData ModelLoader::LoadPrimitive(fastgltf::Asset &gltf, const ModelConfig &config,
                                        fastgltf::Primitive &primitive)
{
    // Retrieve essential info about the mesh primitive:
    auto layout   = GetLayout(config);
    auto vertSize = layout.Stride * sizeof(float);

    const size_t vertCount = GetVertexCount(gltf, primitive);
    const size_t idxCount  = GetIndexCount(gltf, primitive);

    // Allocate memory for vertex and index data:
    const auto spec = GeometrySpec::BuildS<uint32_t>(vertSize, vertCount, idxCount);

    auto geo   = GeometryData(spec);
    geo.Layout = ToGeoLayout(config);

    // Retrieve the data:
    auto vertRes = RetrieveVertices(gltf, primitive, config, geo);
    geo.BBox     = vertRes.BBox;

    RetrieveIndices(gltf, primitive, geo);

    // Generate tangents if necessary:
    if (vertRes.GenerateTangents)
        tangen::GenerateTangents(geo, layout);

    return geo;
}