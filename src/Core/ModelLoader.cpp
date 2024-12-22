#include "ModelLoader.h"

#include "GeometryProvider.h"
#include "VertexLayout.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <iostream>

static fastgltf::Asset GetAsset(const std::filesystem::path &path,
                                bool loadBuffers = false)
{
    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (data.error() != fastgltf::Error::None)
        throw std::runtime_error("Failed to load a gltf file: " + path.string());

    auto loadOptions = fastgltf::Options::None;

    if (loadBuffers)
        loadOptions = fastgltf::Options::LoadExternalBuffers;

    auto load = parser.loadGltf(data.get(), path.parent_path(), loadOptions);

    if (load.error() != fastgltf::Error::None)
        throw std::runtime_error("Failed to load a gltf file: " + path.string());

    return std::move(load.get());
}

fastgltf::Asset ModelLoader::GetGltf(const std::filesystem::path &filepath)
{
    return GetAsset(filepath, false);
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

static void RetrieveVertices(fastgltf::Asset &gltf, fastgltf::Primitive &primitive,
                             const ModelLoader::VertexConfig &config, GeometryData &geo)
{
    uint32_t vertexStride = 3;
    if (config.LoadTexCoord)
        vertexStride += 2;
    if (config.LoadNormals)
        vertexStride += 3;
    if (config.LoadTangents)
        vertexStride += 4;

    auto vertCount = GetVertexCount(gltf, primitive);
    auto compCount = vertCount * vertexStride;

    auto data = new (geo.VertexData.Data.get()) float[compCount];

    // Retrieve the positions
    fastgltf::Accessor &posAccessor = GetAttributeAccessor(gltf, primitive, "POSITION");

    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                  [&](glm::vec3 v, size_t index) {
                                                      auto idx = vertexStride * index;

                                                      data[idx + 0] = v.x;
                                                      data[idx + 1] = v.y;
                                                      data[idx + 2] = v.z;
                                                  });

    // Retrieve texture coords
    auto texcoordIt = primitive.findAttribute("TEXCOORD_0");

    if (config.LoadTexCoord && texcoordIt != primitive.attributes.end())
    {
        uint32_t texOffset = 3;

        fastgltf::Accessor &texcoordAccessor = gltf.accessors[texcoordIt->accessorIndex];

        fastgltf::iterateAccessorWithIndex<glm::vec2>(
            gltf, texcoordAccessor, [&](glm::vec2 v, size_t index) {
                auto idx = vertexStride * index + texOffset;

                data[idx + 0] = v.x;
                data[idx + 1] = v.y;
            });
    }

    // Retrieve normals
    auto normalIt = primitive.findAttribute("NORMAL");

    if (config.LoadNormals && normalIt != primitive.attributes.end())
    {
        uint32_t normOffset = 3;
        if (config.LoadTexCoord)
            normOffset += 2;

        fastgltf::Accessor &normalAccessor = gltf.accessors[normalIt->accessorIndex];

        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            gltf, normalAccessor, [&](glm::vec3 v, size_t index) {
                auto idx = vertexStride * index + normOffset;

                data[idx + 0] = v.x;
                data[idx + 1] = v.y;
                data[idx + 2] = v.z;
            });
    }

    // Retrieve tangents
    auto tangentIt = primitive.findAttribute("TANGENT");

    if (config.LoadTangents && tangentIt != primitive.attributes.end())
    {
        uint32_t tangentOffset = 3;
        if (config.LoadTexCoord)
            tangentOffset += 2;
        if (config.LoadNormals)
            tangentOffset += 3;

        fastgltf::Accessor &tangentAccessor = gltf.accessors[tangentIt->accessorIndex];

        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            gltf, tangentAccessor, [&](glm::vec4 v, size_t index) {
                auto idx = vertexStride * index + tangentOffset;

                data[idx + 0] = v.x;
                data[idx + 1] = v.y;
                data[idx + 2] = v.z;
                data[idx + 3] = v.w;
            });
    }
}

static void RetrieveIndices(fastgltf::Asset &gltf, fastgltf::Primitive &primitive,
                            GeometryData &geo)
{
    auto accessor = gltf.accessors[primitive.indicesAccessor.value()];

    auto Indices = new (geo.IndexData.Data.get()) uint32_t[accessor.count];
    size_t current = 0;

    fastgltf::iterateAccessor<std::uint32_t>(gltf, accessor, [&](std::uint32_t idx) {
        Indices[current] = idx;
        current++;
    });
}

GeometryProvider ModelLoader::LoadModel(const ModelConfig &config)
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{};
    layout.IndexType = VK_INDEX_TYPE_UINT32;

    layout.VertexLayout.push_back(Vec3);

    if (config.Vertex.LoadTexCoord)
        layout.VertexLayout.push_back(Vec2);

    if (config.Vertex.LoadNormals)
        layout.VertexLayout.push_back(Vec3);

    if (config.Vertex.LoadTangents)
        layout.VertexLayout.push_back(Vec4);

    auto geoProvider = [config]() {
        std::vector<GeometryData> res;

        auto gltf = GetAsset(config.Filepath, true);

        for (auto &mesh : gltf.meshes)
        {
            for (auto &primitive : mesh.primitives)
            {
                // Retrieve essential info about the mesh:
                const size_t vertCount = GetVertexCount(gltf, primitive);
                const size_t idxCount = GetIndexCount(gltf, primitive);

                // Allocate memory for vertex and index data:
                const auto spec = GeometrySpec::BuildS<48, uint32_t>(vertCount, idxCount);

                auto &geo = res.emplace_back(spec);

                // Retrieve the data:
                RetrieveVertices(gltf, primitive, config.Vertex, geo);
                RetrieveIndices(gltf, primitive, geo);
            }
        }

        return res;
    };

    return GeometryProvider{
        // config.Filepath.stem().string(),
        layout,
        geoProvider,
    };
}

GeometryProvider ModelLoader::LoadPrimitive(const ModelConfig &config, int64_t meshId,
                                            int64_t primitiveId)
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{.VertexLayout = {Vec3, Vec2, Vec3},
                          .IndexType = VK_INDEX_TYPE_UINT32};

    auto geoProvider = [config, meshId, primitiveId]() {
        std::vector<GeometryData> res;

        // Parse the gltf:
        auto gltf = GetAsset(config.Filepath, true);

        // Select the specified primitive:
        auto &mesh = gltf.meshes[meshId];
        auto &primitive = mesh.primitives[primitiveId];

        // Retrieve essential info about the mesh:
        const size_t vertCount = GetVertexCount(gltf, primitive);
        const size_t idxCount = GetIndexCount(gltf, primitive);

        // Allocate memory for vertex and index data
        const auto spec = GeometrySpec::BuildS<40, uint32_t>(vertCount, idxCount);

        auto &geo = res.emplace_back(spec);

        // Retrieve the data:
        RetrieveVertices(gltf, primitive, config.Vertex, geo);
        RetrieveIndices(gltf, primitive, geo);

        return res;
    };

    return GeometryProvider{
        // config.Filepath.stem().string(),
        layout,
        geoProvider,
    };
}