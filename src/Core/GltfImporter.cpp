#include "GltfImporter.h"
#include "Pch.h"

#include "TangentsGenerator.h"
#include "Vassert.h"
#include "VertexLayout.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <glm/gtc/quaternion.hpp>

#include <iostream>
#include <ranges>

struct VertexLoadFlags {
    bool LoadTexCoord;
    bool LoadNormals;
    bool LoadTangents;
    bool LoadColor;
};

static VertexLoadFlags GetLoadFlags(Vertex::Layout vLayout)
{
    if (auto *layout = std::get_if<Vertex::PushLayout>(&vLayout))
    {
        return VertexLoadFlags{
            .LoadTexCoord = layout->HasTexCoord,
            .LoadNormals  = layout->HasNormal,
            .LoadTangents = layout->HasTangent,
            .LoadColor    = layout->HasColor,
        };
    }

    // All currently supported pull layout represent the same data:
    return VertexLoadFlags{
        .LoadTexCoord = true,
        .LoadNormals  = true,
        .LoadTangents = true,
        .LoadColor    = false,
    };
}

struct GltfAsset::Impl {
    Impl(const std::filesystem::path &path, bool loadBuffers)
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

        Asset = std::move(load.get());
    }

    fastgltf::Asset Asset;
};

GltfAsset::GltfAsset(const std::filesystem::path &filepath)
{
    mPImpl = std::make_unique<GltfAsset::Impl>(filepath, true);
}

GltfAsset::~GltfAsset()
{
    mPImpl.reset(nullptr);
}

GltfAsset::GltfAsset(GltfAsset &&other) noexcept
{
    mPImpl = std::move(other.mPImpl);
}

GltfAsset &GltfAsset::operator=(GltfAsset &&other) noexcept
{
    mPImpl = std::move(other.mPImpl);
    return *this;
}

// Utility function to obtain absolute paths to gltf-referenced images.
// Templated, because it handles several types from fastgltf.
template <typename T>
static auto GetTexturePath(fastgltf::Asset &gltf, std::optional<T> &texInfo,
                           const std::filesystem::path &workingDir)
    -> std::optional<std::filesystem::path>
{
    // 1. Check if info has value
    if (!texInfo.has_value())
        return std::nullopt;

    // 2. Retrieve image index if present
    auto imgId = gltf.textures[texInfo->textureIndex].imageIndex;

    if (!imgId.has_value())
        return std::nullopt;

    // 3. Get data source
    auto &dataSource = gltf.images[imgId.value()].data;

    // 4. Retrieve the URI if present;
    if (!std::holds_alternative<fastgltf::sources::URI>(dataSource))
        return std::nullopt;

    auto &uri = std::get<fastgltf::sources::URI>(dataSource);

    // 5. Retrieve the filepath:
    return workingDir / uri.uri.fspath();
}

// Small utility to convert from normalized float
// to uint_8 representation for pixels:
static uint8_t PixelChannelFromFloat(float x)
{
    return static_cast<uint8_t>(255.0f * x);
}

void GltfAsset::PreprocessMaterials(Scene &scene, std::map<size_t, SceneKey> &keyMap,
                                    std::vector<ImageTaskData> &tasks,
                                    const ModelConfig          &config)
{
    using namespace std::views;

    vassert(keyMap.empty(), "Key map should be empty!");
    vassert(tasks.empty(), "Tasks vector should be empty!");

    const std::filesystem::path workingDir = config.Filepath.parent_path();
    const std::string           baseName   = config.Filepath.stem().string();

    // Loop over all materials in gltf:
    for (auto [id, material] : enumerate(mPImpl->Asset.materials))
    {
        // Create new scene material:
        auto [matKey, mat] = scene.EmplaceMaterial();
        mat.Name           = baseName + std::to_string(id);

        keyMap[id] = matKey;

        // Load alpha cutoff where applicable
        if (material.alphaMode == fastgltf::AlphaMode::Mask)
        {
            mat.AlphaCutoff = material.alphaCutoff;
        }

        // Load info about double-sidedness:
        mat.DoubleSided = material.doubleSided;

        // Load information about translucency/diffuse transmission:
        if (material.diffuseTransmission)
        {
            auto color = material.diffuseTransmission->diffuseTransmissionColorFactor;

            mat.TranslucentColor = glm::vec3(color.x(), color.y(), color.z());
        }

        // Handle albedo:
        {
            auto [imgKey, img] = scene.EmplaceImage();
            mat.Albedo         = imgKey;

            auto &albedoInfo = material.pbrData.baseColorTexture;
            auto  albedoPath = GetTexturePath(mPImpl->Asset, albedoInfo, workingDir);

            auto &fac = material.pbrData.baseColorFactor;

            auto baseColor = Pixel{
                .R = PixelChannelFromFloat(fac.x()),
                .G = PixelChannelFromFloat(fac.y()),
                .B = PixelChannelFromFloat(fac.z()),
                .A = PixelChannelFromFloat(fac.w()),
            };

            tasks.push_back(ImageTaskData{
                .ImageKey  = imgKey,
                .Path      = albedoPath,
                .BaseColor = baseColor,
                .Name      = mat.Name + " Albedo",
                .Unorm     = false,
            });
        }

        // Do the same for roughness/metallic:
        if (config.FetchRoughness)
        {
            auto [imgKey, img] = scene.EmplaceImage();
            mat.Roughness      = imgKey;

            auto &roughnessInfo = material.pbrData.metallicRoughnessTexture;
            auto roughnessPath = GetTexturePath(mPImpl->Asset, roughnessInfo, workingDir);

            auto baseColor = Pixel{
                .R = PixelChannelFromFloat(0.0f),
                .G = PixelChannelFromFloat(material.pbrData.roughnessFactor),
                .B = PixelChannelFromFloat(material.pbrData.metallicFactor),
                .A = PixelChannelFromFloat(0.0f),
            };

            tasks.push_back(ImageTaskData{
                .ImageKey  = imgKey,
                .Path      = roughnessPath,
                .BaseColor = baseColor,
                .Name      = mat.Name + " Roughness",
                .Unorm     = true,
            });
        }

        // Do the same for normal map if requested:
        if (config.FetchNormal)
        {
            auto &normalInfo = material.normalTexture;
            auto  normalPath = GetTexturePath(mPImpl->Asset, normalInfo, workingDir);

            if (normalPath.has_value())
            {
                auto [imgKey, img] = scene.EmplaceImage();
                mat.Normal         = imgKey;

                tasks.push_back(ImageTaskData{
                    .ImageKey  = imgKey,
                    .Path      = normalPath,
                    .BaseColor = Pixel{},
                    .Name      = mat.Name + " Normal",
                    .Unorm     = true,
                });
            }
        }
    }
}

void GltfAsset::PreprocessMeshes(Scene &scene, std::map<size_t, SceneKey> &meshKeyMap,
                                 std::vector<PrimitiveTaskData>   &tasks,
                                 const ModelConfig                &config,
                                 const std::map<size_t, SceneKey> &matKeyMap)
{
    using namespace std::views;

    vassert(meshKeyMap.empty(), "Key map should be empty!");
    vassert(tasks.empty(), "Tasks vector should be empty!");

    const std::string baseName = config.Filepath.stem().string();

    // Iterate all gltf meshes:
    for (auto [gltfMeshId, gltfMesh] : enumerate(mPImpl->Asset.meshes))
    {
        // Create the new mesh:
        auto [meshKey, mesh] = scene.EmplaceMesh();
        mesh.Name            = std::format("{} {}", baseName, gltfMesh.name);

        // Update mesh key map:
        meshKeyMap[gltfMeshId] = meshKey;

        // Retrieve its primitives:
        for (auto [gltfPrimId, gltfPrim] : enumerate(gltfMesh.primitives))
        {
            // Emplace new primitive:
            auto &newMeshPrim = mesh.Primitives.emplace_back();

            // Assign material keys to the mesh:
            if (auto id = gltfPrim.materialIndex)
            {
                auto matId           = matKeyMap.at(*id);
                newMeshPrim.Material = matId;
            }

            tasks.push_back(PrimitiveTaskData{
                .SceneMesh = meshKey,
                .ScenePrim = mesh.Primitives.size() - 1,
                .GltfMesh  = gltfMeshId,
                .GltfPrim  = gltfPrimId,
            });
        }
    }
}

static auto UnpackTransform(fastgltf::Node &node)
    -> std::tuple<glm::vec3, glm::vec3, glm::vec3>
{
    fastgltf::TRS trs;

    if (std::holds_alternative<fastgltf::TRS>(node.transform))
    {
        trs = std::get<fastgltf::TRS>(node.transform);
    }
    else
    {
        auto &mat = std::get<fastgltf::math::fmat4x4>(node.transform);
        fastgltf::math::decomposeTransformMatrix(mat, trs.scale, trs.rotation,
                                                 trs.translation);
    }

    auto &t = trs.translation;
    auto &r = trs.rotation;
    auto &s = trs.scale;

    auto translation = glm::vec3(t.x(), t.y(), t.z());

    auto quat     = glm::quat(r.w(), r.x(), r.y(), r.z());
    auto rotation = glm::eulerAngles(quat);

    auto scale = glm::vec3(s.x(), s.y(), s.z());

    return {translation, rotation, scale};
}

void GltfAsset::PreprocessHierarchy(SceneGraphNode                   &root,
                                    const std::map<size_t, SceneKey> &meshKeyMap)
{
    // TODO: Currently we assume gltf holds one scene.
    auto &scene = mPImpl->Asset.scenes[0];

    for (auto nodeIdx : scene.nodeIndices)
    {
        auto &node = mPImpl->Asset.nodes[nodeIdx];

        // TODO: Only handles first-level nodes that hold meshes
        if (!node.meshIndex.has_value())
            return;

        size_t meshIdx = *node.meshIndex;
        auto   meshKey = meshKeyMap.at(meshIdx);

        auto [translation, rotation, scale] = UnpackTransform(node);

        auto &prefabNode       = root.EmplaceChild(meshKey);
        prefabNode.Translation = translation;
        prefabNode.Rotation    = rotation;
        prefabNode.Scale       = scale;
        prefabNode.Name        = node.name;
    }
}

static fastgltf::Accessor &GetAttributeAccessor(fastgltf::Asset     &gltf,
                                                fastgltf::Primitive &primitive,
                                                std::string_view     name)
{
    return gltf.accessors[primitive.findAttribute(name)->accessorIndex];
}

static size_t GetVertexCount(fastgltf::Asset &gltf, fastgltf::Primitive &primitive)
{
    fastgltf::Accessor &posAccessor = GetAttributeAccessor(gltf, primitive, "POSITION");
    return posAccessor.count;
}

static size_t GetIndexCount(fastgltf::Asset &gltf, fastgltf::Primitive &primitive)
{
    auto &indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
    return indexAccessor.count;
}

PrimitiveData GltfAsset::LoadPrimitive(PrimitiveTaskData data, const ModelConfig &config)
{
    PrimitiveData res{};

    auto &gltf      = mPImpl->Asset;
    auto &mesh      = gltf.meshes[data.GltfMesh];
    auto &primitive = mesh.primitives[data.GltfPrim];

    auto flags = GetLoadFlags(config.VertexLayout);

    // Retrieve indices:
    res.IndexCount = GetIndexCount(gltf, primitive);

    {
        res.Indices.resize(res.IndexCount);

        auto &indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];

        size_t current = 0;

        fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
                                                 [&](std::uint32_t idx) {
                                                     res.Indices[current] = idx;
                                                     current++;
                                                 });
    }

    // Retrieve vertex attributes:
    res.VertexCount = GetVertexCount(gltf, primitive);

    // Retrieve the positions and calculate bounding box in one go:
    {
        res.Positions.resize(res.VertexCount);

        auto minCoords = glm::vec3(std::numeric_limits<float>::max());
        auto maxCoords = glm::vec3(std::numeric_limits<float>::lowest());

        fastgltf::Accessor &posAccessor =
            GetAttributeAccessor(gltf, primitive, "POSITION");

        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            gltf, posAccessor, [&](glm::vec3 v, size_t index) {
                res.Positions[index] = v;

                minCoords = glm::min(minCoords, v);
                maxCoords = glm::max(maxCoords, v);
            });

        res.BBox.Center = 0.5f * (maxCoords + minCoords);
        res.BBox.Extent = 0.5f * (maxCoords - minCoords);
    }

    // Retrieve texture coords
    if (flags.LoadTexCoord)
    {
        auto texcoordIt = primitive.findAttribute("TEXCOORD_0");

        if (texcoordIt != primitive.attributes.end())
        {
            res.TexCoords.resize(res.VertexCount);

            auto minCoords = glm::vec2(std::numeric_limits<float>::max());
            auto maxCoords = glm::vec2(std::numeric_limits<float>::lowest());

            fastgltf::Accessor &texcoordAccessor =
                gltf.accessors[texcoordIt->accessorIndex];

            fastgltf::iterateAccessorWithIndex<glm::vec2>(
                gltf, texcoordAccessor, [&](glm::vec2 v, size_t index) {
                    res.TexCoords[index] = v;

                    minCoords = glm::min(minCoords, v);
                    maxCoords = glm::max(maxCoords, v);
                });

            glm::vec2 center = 0.5f * (maxCoords + minCoords);
            glm::vec2 extent = 0.5f * (maxCoords - minCoords);

            if (extent.x != 0.0 && extent.y != 0.0)
            {
                res.TexBounds.Center = center;
                res.TexBounds.Extent = extent;
            }
            else
            {
                std::cerr << "In gltf file " << config.Filepath.string()
                          << " mesh: " << data.GltfMesh << " prim: " << data.GltfPrim
                          << "degenerate texcoord range: " 
                          << minCoords.x << ", " << minCoords.y
                          << "to "
                          << maxCoords.x << ", " << maxCoords.y
                          << '\n';
            }
        }

        else
        {
            std::cerr << "Gltf file: " << config.Filepath.string()
                      << "primitive doesn't contain texture coordinates.\n";
        }
    }

    // Retrieve normals
    if (flags.LoadNormals)
    {
        auto normalIt = primitive.findAttribute("NORMAL");

        if (normalIt != primitive.attributes.end())
        {
            res.Normals.resize(res.VertexCount);

            fastgltf::Accessor &normalAccessor = gltf.accessors[normalIt->accessorIndex];

            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                gltf, normalAccessor,
                [&](glm::vec3 v, size_t index) { res.Normals[index] = v; });
        }

        else
        {
            std::cerr << "Gltf file: " << config.Filepath.string()
                      << "primitive doesn't contain normals.\n";
        }
    }

    // Retrieve tangents if present, generate them with mikktspace otherwise
    if (flags.LoadTangents)
    {
        auto tangentIt = primitive.findAttribute("TANGENT");

        if (tangentIt != primitive.attributes.end())
        {
            res.Tangents.resize(res.VertexCount);

            fastgltf::Accessor &tangentAccessor =
                gltf.accessors[tangentIt->accessorIndex];

            fastgltf::iterateAccessorWithIndex<glm::vec4>(
                gltf, tangentAccessor,
                [&](glm::vec4 v, size_t index) { res.Tangents[index] = v; });
        }
        else
        {
            tangen::GenerateTangents(res);
        }
    }

    // TODO: Fetch colors?

    return res;
}