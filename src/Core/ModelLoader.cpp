#include "ModelLoader.h"
#include "Pch.h"

#include "TangentsGenerator.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include "Vassert.h"

#include <iostream>

static fastgltf::Asset GetAsset(const std::filesystem::path &path,
                                bool loadBuffers = false)
{
    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

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

static fastgltf::Accessor &GetAttributeAccessor(fastgltf::Asset &gltf,
                                                fastgltf::Primitive &primitive,
                                                std::string_view name)
{
    return gltf.accessors[primitive.findAttribute(name)->accessorIndex];
}

static size_t GetIndexCount(fastgltf::Asset &gltf, fastgltf::Primitive &primitive)
{
    auto indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
    return indexAccessor.count;
}

static size_t GetVertexCount(fastgltf::Asset &gltf, fastgltf::Primitive &primitive)
{
    fastgltf::Accessor &posAccessor = GetAttributeAccessor(gltf, primitive, "POSITION");
    return posAccessor.count;
}

static tangen::VertexLayout GetLayout(const ModelConfig &config)
{
    tangen::VertexLayout res{
        .Stride = 3,
        .OffsetTexCoord = 3,
        .OffsetNormal = 3,
        .OffsetTangent = 3,
    };

    if (config.LoadTexCoord)
    {
        res.Stride += 2;
        res.OffsetNormal += 2;
        res.OffsetTangent += 2;
    }

    if (config.LoadNormals)
    {
        res.Stride += 3;
        res.OffsetTangent += 3;
    }

    if (config.LoadTangents)
    {
        res.Stride += 4;
    }

    return res;
}

static GeometryLayout ToGeoLayout(const ModelConfig &config)
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{};

    layout.VertexLayout.push_back(Vec3);

    if (config.LoadTexCoord)
        layout.VertexLayout.push_back(Vec2);

    if (config.LoadNormals)
        layout.VertexLayout.push_back(Vec3);

    if (config.LoadTangents)
        layout.VertexLayout.push_back(Vec4);

    layout.IndexType = VK_INDEX_TYPE_UINT32;

    return layout;
}

struct VertParsingResult {
    bool GenerateTangents = false;
};

static VertParsingResult RetrieveVertices(fastgltf::Asset &gltf,
                                          fastgltf::Primitive &primitive,
                                          const ModelConfig &config, GeometryData &geo)
{
    VertParsingResult res{};

    auto layout = GetLayout(config);

    auto vertCount = GetVertexCount(gltf, primitive);
    auto compCount = vertCount * layout.Stride;

    auto data = new (geo.VertexData.Data) float[compCount];

    // Retrieve the positions
    fastgltf::Accessor &posAccessor = GetAttributeAccessor(gltf, primitive, "POSITION");

    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                  [&](glm::vec3 v, size_t index) {
                                                      auto idx = layout.Stride * index;

                                                      data[idx + 0] = v.x;
                                                      data[idx + 1] = v.y;
                                                      data[idx + 2] = v.z;
                                                  });

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
                    auto idx = layout.Stride * index + layout.OffsetTexCoord;

                    data[idx + 0] = v.x;
                    data[idx + 1] = v.y;
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
                    auto idx = layout.Stride * index + layout.OffsetNormal;

                    data[idx + 0] = v.x;
                    data[idx + 1] = v.y;
                    data[idx + 2] = v.z;
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
                    auto idx = layout.Stride * index + layout.OffsetTangent;

                    data[idx + 0] = v.x;
                    data[idx + 1] = v.y;
                    data[idx + 2] = v.z;
                    data[idx + 3] = v.w;
                });
        }
        else
        {
            res.GenerateTangents = true;
        }
    }

    return res;
}

static void RetrieveIndices(fastgltf::Asset &gltf, fastgltf::Primitive &primitive,
                            GeometryData &geo)
{
    auto accessor = gltf.accessors[primitive.indicesAccessor.value()];

    auto Indices = new (geo.IndexData.Data) uint32_t[accessor.count];
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
    auto layout = GetLayout(config);
    auto vertSize = layout.Stride * sizeof(float);

    const size_t vertCount = GetVertexCount(gltf, primitive);
    const size_t idxCount = GetIndexCount(gltf, primitive);

    // Allocate memory for vertex and index data:
    const auto spec = GeometrySpec::BuildS<uint32_t>(vertSize, vertCount, idxCount);

    auto geo = GeometryData(spec);

    // Retrieve the data:
    auto vertRes = RetrieveVertices(gltf, primitive, config, geo);
    RetrieveIndices(gltf, primitive, geo);

    // Generate tangents if necessary:
    if (vertRes.GenerateTangents)
        tangen::GenerateTangents(geo, layout);

    geo.Layout = ToGeoLayout(config);

    return geo;
}