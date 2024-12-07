#include "ModelLoader.h"
#include "GeometryProvider.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

static fastgltf::Asset GetAsset(const std::filesystem::path &path,
                                bool loadBuffers = false)
{
    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (data.error() != fastgltf::Error::None)
    {
        std::string err_msg = "Failed to load a gltf file!\n";
        err_msg += "Filepath: " + path.string();

        throw std::runtime_error(err_msg);
    }

    auto loadOptions = fastgltf::Options::None;

    if (loadBuffers)
        loadOptions = fastgltf::Options::LoadExternalBuffers;

    auto load = parser.loadGltf(data.get(), path.parent_path(), loadOptions);

    if (load.error() != fastgltf::Error::None)
    {
        std::string err_msg = "Failed to parse a gltf file!\n";
        err_msg += "Filepath: " + path.string();

        throw std::runtime_error(err_msg);
    }

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

static fastgltf::Accessor &GetIndexAccessor(fastgltf::Asset &gltf,
                                            fastgltf::Primitive &primitive)
{
    return gltf.accessors[primitive.indicesAccessor.value()];
}

GeometryProvider ModelLoader::LoadPrimitive(const Config &config)
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{.VertexLayout = {Vec3, Vec2, Vec3},
                          .IndexType = VK_INDEX_TYPE_UINT32};

    auto geoProvider = [config]() {
        std::vector<GeometryData> res;

        // Parse the gltf:
        auto gltf = GetAsset(config.Filepath, true);

        // Temporary, load just the first mesh:
        auto &mesh = gltf.meshes[config.MeshId];
        // Temporary, load just the first mesh primitive:
        auto &primitive = mesh.primitives[config.PrimitiveId];

        struct Vertex {
            glm::vec3 Position;
            glm::vec2 TexCoord;
            glm::vec3 Normal;
        };

        // Retrieve essential info about the mesh:
        fastgltf::Accessor &posAccessor =
            GetAttributeAccessor(gltf, primitive, "POSITION");

        const size_t vertCount = posAccessor.count;

        fastgltf::Accessor &indexAccessor = GetIndexAccessor(gltf, primitive);

        const size_t idxCount = indexAccessor.count;

        // Allocate memory for vertex and index data
        const auto spec = GeometrySpec::Build<Vertex, uint32_t>(vertCount, idxCount);
        auto &geo = res.emplace_back(spec);

        // Retrieve the vertex data
        auto Vertices = new (geo.VertexData.Data.get()) Vertex[vertCount];

        // Retrieve the positions
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            gltf, posAccessor, [&](glm::vec3 v, size_t index) {
                Vertices[index].Position = v;
                Vertices[index].TexCoord = {0.0f, 0.0f};
                Vertices[index].Normal = {0.0f, 0.0f, 0.0f};
            });

        // Retrieve texture coords
        auto texcoordIt = primitive.findAttribute("TEXCOORD_0");

        if (texcoordIt != primitive.attributes.end())
        {
            fastgltf::Accessor &texcoordAccessor =
                gltf.accessors[texcoordIt->accessorIndex];

            fastgltf::iterateAccessorWithIndex<glm::vec2>(
                gltf, texcoordAccessor,
                [&](glm::vec2 v, size_t index) { Vertices[index].TexCoord = v; });
        }

        // Retrieve index data:
        auto Indices = new (geo.IndexData.Data.get()) uint32_t[idxCount];

        {
            size_t current = 0;

            fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
                                                     [&](std::uint32_t idx) {
                                                         Indices[current] = idx;
                                                         current++;
                                                     });
        }

        return res;
    };

    return GeometryProvider{
        // config.Filepath.stem().string(),
        layout,
        geoProvider,
    };
}

GeometryProvider ModelLoader::LoadModel(const std::filesystem::path &filepath)
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{.VertexLayout = {Vec3, Vec2, Vec3},
                          .IndexType = VK_INDEX_TYPE_UINT32};

    auto geoProvider = [filepath]() {
        std::vector<GeometryData> res;

        auto gltf = GetAsset(filepath, true);

        for (auto &mesh : gltf.meshes)
        {
            for (auto &primitive : mesh.primitives)
            {
                struct Vertex {
                    glm::vec3 Position;
                    glm::vec2 TexCoord;
                    glm::vec3 Normal;
                };

                // Retrieve essential info about the mesh:
                fastgltf::Accessor &posAccessor =
                    GetAttributeAccessor(gltf, primitive, "POSITION");

                const size_t vertCount = posAccessor.count;

                fastgltf::Accessor &indexAccessor = GetIndexAccessor(gltf, primitive);

                const size_t idxCount = indexAccessor.count;

                // Allocate memory for vertex and index data
                const auto spec =
                    GeometrySpec::Build<Vertex, uint32_t>(vertCount, idxCount);
                auto &geo = res.emplace_back(spec);

                // Retrieve the vertex data
                auto Vertices = new (geo.VertexData.Data.get()) Vertex[vertCount];

                // Retrieve the positions
                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    gltf, posAccessor, [&](glm::vec3 v, size_t index) {
                        Vertices[index].Position = v;
                        Vertices[index].TexCoord = {0.0f, 0.0f};
                        Vertices[index].Normal = {0.0f, 0.0f, 0.0f};
                    });

                // Retrieve texture coords
                auto texcoordIt = primitive.findAttribute("TEXCOORD_0");

                if (texcoordIt != primitive.attributes.end())
                {
                    fastgltf::Accessor &texcoordAccessor =
                        gltf.accessors[texcoordIt->accessorIndex];

                    fastgltf::iterateAccessorWithIndex<glm::vec2>(
                        gltf, texcoordAccessor,
                        [&](glm::vec2 v, size_t index) { Vertices[index].TexCoord = v; });
                }

                // Retrieve index data:
                auto Indices = new (geo.IndexData.Data.get()) uint32_t[idxCount];

                {
                    size_t current = 0;

                    fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
                                                             [&](std::uint32_t idx) {
                                                                 Indices[current] = idx;
                                                                 current++;
                                                             });
                }
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