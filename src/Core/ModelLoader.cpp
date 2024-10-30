#include "ModelLoader.h"

#include <filesystem>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

void TexturedVertexLoader::LoadGltf(const std::string& filepath)
{
    Vertices.clear();
    Indices.clear();

    auto working_dir = std::filesystem::current_path();
    std::filesystem::path path = working_dir / filepath;

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

    auto gltf = std::move(load.get());

    // Temporary, load just the first mesh
    auto &mesh = gltf.meshes[0];

    //for (auto &&primitive : mesh.primitives)
    {
        //Temporary, load just the first mesh primitive
        auto& primitive = mesh.primitives[0];

        //auto indexCount = gltf.accessors[primitive.indicesAccessor.value()].count;

        //GeoSurface newSurf{
        //    .StartIndex = static_cast<uint32_t>(indices.size()),
        //    .Count = static_cast<uint32_t>(indexCount),
        //};

        //mSurfaces.push_back(newSurf);

        //size_t initial_vtx = vertices.size();

        // Retrieve indices
        {
            fastgltf::Accessor &indexaccessor =
                gltf.accessors[primitive.indicesAccessor.value()];
            Indices.reserve(Indices.size() + indexaccessor.count);

            fastgltf::iterateAccessor<std::uint32_t>(
                gltf, indexaccessor, [&](std::uint32_t idx) {
                    Indices.push_back(idx /*+ static_cast<uint32_t>(initial_vtx)*/);
                });
        }

        // Retrieve vertex positions
        {
            fastgltf::Accessor &posAccessor =
                gltf.accessors[primitive.findAttribute("POSITION")->accessorIndex];
            Vertices.resize(Vertices.size() + posAccessor.count);

            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                gltf, posAccessor, [&](glm::vec3 v, size_t index) {
                    Vertex_PXN newVert;
                    newVert.Position = v;
                    newVert.TexCoord = {0.0f, 0.0f};
                    newVert.Normal = {0.0f, 0.0f, 0.0f};

                    Vertices[/*initial_vtx +*/ index] = newVert;
                });
        }

        // Retrieve texture coords
        auto texcoordIt = primitive.findAttribute("TEXCOORD_0");

        if (texcoordIt != primitive.attributes.end())
        {
            fastgltf::Accessor &texcoordAccessor =
                gltf.accessors[texcoordIt->accessorIndex];

            fastgltf::iterateAccessorWithIndex<glm::vec2>(
                gltf, texcoordAccessor, [&](glm::vec2 v, size_t index) {
                    Vertices[/*initial_vtx +*/ index].TexCoord = v;
                });
        }
    }
}

GeometryProvider TexturedVertexLoader::GetProvider()
{
    auto VertProvider = [this](){return Vertices;};
    auto IdxProvider = [this](){return Indices;};

    return {
        VertProvider, IdxProvider
    };
}