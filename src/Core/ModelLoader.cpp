#include "ModelLoader.h"
#include "GeometryProvider.h"

#include <filesystem>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

static fastgltf::Asset GetAsset(const std::filesystem::path &path)
{
    fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (data.error() != fastgltf::Error::None)
    {
        std::string err_msg = "Failed to load a gltf file!\n";
        err_msg += "Filepath: " + path.string();

        throw std::runtime_error(err_msg);
    }

    auto loadOptions = fastgltf::Options::LoadExternalBuffers;

    auto load = parser.loadGltf(data.get(), path.parent_path(), loadOptions);

    if (load.error() != fastgltf::Error::None)
    {
        std::string err_msg = "Failed to parse a gltf file!\n";
        err_msg += "Filepath: " + path.string();

        throw std::runtime_error(err_msg);
    }

    return std::move(load.get());
}

GeometryProvider ModelLoader::PosTex(const std::string &filepath)
{
    auto working_dir = std::filesystem::current_path();
    std::filesystem::path path = working_dir / filepath;

    using enum Vertex::AttributeType;

    GeometryLayout layout{.VertexLayout = {Vec3, Vec2, Vec3},
                          .IndexType = VK_INDEX_TYPE_UINT32};

    auto geoProvider = [path]() {
        auto gltf = GetAsset(path);

        // Temporary, load just the first mesh:
        auto &mesh = gltf.meshes[0];
        // Temporary, load just the first mesh primitive:
        auto &primitive = mesh.primitives[0];

        // Retrieve the vertex data
        fastgltf::Accessor &posAccessor =
            gltf.accessors[primitive.findAttribute("POSITION")->accessorIndex];

        struct Vertex {
            glm::vec3 Position;
            glm::vec2 TexCoord;
            glm::vec3 Normal;
        };

        const size_t vertCount = posAccessor.count;
        const size_t vertSize = vertCount * sizeof(Vertex);

        OpaqueBuffer vertBuf(vertCount, vertSize, 4);

        auto Vertices = new (vertBuf.Data.get()) Vertex[vertCount];

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
        fastgltf::Accessor &indexAccessor =
            gltf.accessors[primitive.indicesAccessor.value()];

        const size_t idxCount = indexAccessor.count;
        const size_t idxSize = idxCount * sizeof(uint32_t);

        OpaqueBuffer idxBuf(idxCount, idxSize, 4);

        auto Indices = new (idxBuf.Data.get()) uint32_t[idxCount];

        {
            size_t current = 0;

            fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
                                                     [&](std::uint32_t idx) {
                                                         Indices[current] = idx;
                                                         current++;
                                                     });
        }

        return GeometryData{std::move(vertBuf), std::move(idxBuf)};
    };

    return GeometryProvider{
        path.stem().string(),
        layout,
        geoProvider,
    };
}