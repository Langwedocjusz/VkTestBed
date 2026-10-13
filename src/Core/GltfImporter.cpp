#include "GltfImporter.h"

#include "Pch.h"

#include "Path.h"
#include "TangentsGenerator.h"
#include "Vassert.h"
#include "VertexLayout.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>

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

    // All currently supported pull layouts represent the same data:
    return VertexLoadFlags{
        .LoadTexCoord = true,
        .LoadNormals  = true,
        .LoadTangents = true,
        .LoadColor    = false,
    };
}


// NOTE: Using custom callbacks, since default cgltf path would use fopen,
// which by default fails on utf8 paths on windows:
static cgltf_result Utf8FileRead(const cgltf_memory_options *memOpts,
                                 const cgltf_file_options *, const char *path,
                                 cgltf_size *size, void **data)
{
    Path pathObj{path};

    std::ifstream file = pathObj.Open(std::ios::binary | std::ios::ate);

    if (!file)
        return cgltf_result_file_not_found;

    const auto fileSize = static_cast<cgltf_size>(file.tellg());
    file.seekg(0, std::ios::beg);

    void *mem = memOpts->alloc_func ? memOpts->alloc_func(memOpts->user_data, fileSize)
                                    : std::malloc(fileSize);

    if (!mem)
        return cgltf_result_out_of_memory;

    if (!file.read(static_cast<char *>(mem), static_cast<std::streamsize>(fileSize)))
    {
        if (memOpts->free_func)
            memOpts->free_func(memOpts->user_data, mem);
        else
            std::free(mem);

        return cgltf_result_io_error;
    }

    *size = fileSize;
    *data = mem;

    return cgltf_result_success;
}

static void Utf8FileRelease(const cgltf_memory_options *memOpts,
                            const cgltf_file_options *, void *data, cgltf_size)
{
    if (memOpts->free_func)
        memOpts->free_func(memOpts->user_data, data);
    else
        std::free(data);
}

struct GltfAsset::Impl {
    Impl(const std::string &path, bool loadBuffers)
    {
        cgltf_options options{};
        options.file.read = &Utf8FileRead;
        options.file.release = &Utf8FileRelease;

        auto parse = cgltf_parse_file(&options, path.c_str(), &Data);

        if (parse != cgltf_result_success)
        {
            Data = nullptr;
            vassert(false, "Failed to parse a gltf file: " + path +
                               " (cgltf_result: " + std::to_string(static_cast<int>(parse)) + ")");
        }

        if (loadBuffers)
        {
            auto load = cgltf_load_buffers(&options, Data, path.c_str());

            if (load != cgltf_result_success)
            {
                cgltf_free(Data);
                Data = nullptr;
                vassert(false, "Failed to load buffers of a gltf file: " + path +
                                   " (cgltf_result: " + std::to_string(static_cast<int>(load)) + ")");
            }
        }

        // Optional, but cheap: catches out-of-range indices, bad accessors etc.,
        // so that the unchecked indexing done later is safe.
        if (cgltf_validate(Data) != cgltf_result_success)
        {
            cgltf_free(Data);
            Data = nullptr;
            vassert(false, "Gltf file failed validation: " + path);
        }
    }

    ~Impl()
    {
        if (Data)
            cgltf_free(Data);
    }

    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;

    cgltf_data *Data = nullptr;
};

GltfAsset::GltfAsset(const std::string &filepath)
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

// cgltf uses nullptr to denote lack of optional strings:
static std::string SafeName(const char *name)
{
    return name ? std::string(name) : std::string();
}

// Utility function to obtain absolute paths to gltf-referenced images.
static auto GetTexturePath(const cgltf_texture_view &view,
                           const Path &workingDir)
    -> std::optional<std::string>
{
    // 1. Check if the texture is present at all
    if (view.texture == nullptr)
        return std::nullopt;

    // 2. Retrieve image if present
    const cgltf_image *image = view.texture->image;

    if (image == nullptr)
        return std::nullopt;

    // 3. Retrieve image uri:
    if (image->uri == nullptr)
        return std::nullopt;

    // NOTE: Currently no support for embedded images:
    if (std::string_view(image->uri).starts_with("data:"))
    {
        std::cout << "Embedded image found when importing from " << workingDir.String() << ". It will be skipped. \n";
        return std::nullopt;
    }

    // NOTE: URIs are percent-encoded, cgltf decodes in place:
    std::string decoded(image->uri);
    decoded.resize(cgltf_decode_uri(decoded.data()));

    // 4. Reconstruct the full image path:
    Path relPath{decoded};

    Path fullPath = workingDir / relPath;

    return fullPath.String();
}

// Small utility to convert from normalized float
// to uint_8 representation for pixels:
static uint8_t PixelChannelFromFloat(float x)
{
    return static_cast<uint8_t>(255.0f * x);
}

void GltfAsset::PreprocessMaterials(Scene &scene, std::map<size_t, SceneKey> &keyMap,
                                    std::vector<ImageTaskData> &tasks,
                                    const ModelConfig &config)
{
    vassert(keyMap.empty(), "Key map should be empty!");
    vassert(tasks.empty(), "Tasks vector should be empty!");

    const cgltf_data *gltf = mPImpl->Data;

    Path path{config.Filepath};
    Path workingDir = path.Parent(); // Is this a dangerous assumption?
    std::string baseName = path.Stem();

    // Loop over all materials in gltf:
    for (size_t id = 0; id < gltf->materials_count; id++)
    {
        const cgltf_material &material = gltf->materials[id];

        // Create new scene material:
        auto [matKey, mat] = scene.EmplaceMaterial();

        mat.Name = baseName + std::to_string(id);
        keyMap[id] = matKey;

        // Load alpha information:
        switch (material.alpha_mode)
        {
        case cgltf_alpha_mode_opaque: {
            mat.AlphaMode = MaterialAlphaMode::Opaque;
            break;
        }
        case cgltf_alpha_mode_mask: {
            mat.AlphaMode = MaterialAlphaMode::Mask;
            mat.AlphaCutoff = material.alpha_cutoff;
            break;
        }
        case cgltf_alpha_mode_blend: {
            mat.AlphaMode = MaterialAlphaMode::Blend;
            break;
        }
        default:
            break;
        }

        // Load info about double-sidedness:
        mat.DoubleSided = material.double_sided;

        // Load information about translucency/diffuse transmission:
        if (material.has_diffuse_transmission)
        {
            const auto &color = material.diffuse_transmission.diffuse_transmission_color_factor;
            mat.TranslucentColor = glm::vec3(color[0], color[1], color[2]);
        }

        // Initialize to gltf spec defaults:
        cgltf_pbr_metallic_roughness pbr{
            .base_color_texture         = {},
            .metallic_roughness_texture = {},
            .base_color_factor          = {1.0f, 1.0f, 1.0f, 1.0f},
            .metallic_factor            = 1.0f,
            .roughness_factor           = 1.0f,
        };

        if (material.has_pbr_metallic_roughness)
            pbr = material.pbr_metallic_roughness;

        // Handle albedo:
        {
            auto [imgKey, img] = scene.EmplaceImage();
            mat.Albedo = imgKey;

            auto albedoPath = GetTexturePath(pbr.base_color_texture, workingDir);

            const auto &fac = pbr.base_color_factor;

            auto baseColor = Pixel{
                .R = PixelChannelFromFloat(fac[0]),
                .G = PixelChannelFromFloat(fac[1]),
                .B = PixelChannelFromFloat(fac[2]),
                .A = PixelChannelFromFloat(fac[3]),
            };

            tasks.push_back(ImageTaskData{
                .ImageKey = imgKey,
                .Path = albedoPath,
                .BaseColor = baseColor,
                .Name = mat.Name + " Albedo",
                .Unorm = false,
            });
        }

        // Do the same for roughness/metallic:
        if (config.FetchRoughness)
        {
            auto [imgKey, img] = scene.EmplaceImage();
            mat.Roughness = imgKey;

            auto roughnessPath = GetTexturePath(pbr.metallic_roughness_texture, workingDir);

            auto baseColor = Pixel{
                .R = PixelChannelFromFloat(0.0f),
                .G = PixelChannelFromFloat(pbr.roughness_factor),
                .B = PixelChannelFromFloat(pbr.metallic_factor),
                .A = PixelChannelFromFloat(0.0f),
            };

            tasks.push_back(ImageTaskData{
                .ImageKey = imgKey,
                .Path = roughnessPath,
                .BaseColor = baseColor,
                .Name = mat.Name + " Roughness",
                .Unorm = true,
            });
        }

        // Do the same for normal map if requested:
        if (config.FetchNormal)
        {
            auto normalPath = GetTexturePath(material.normal_texture, workingDir);

            if (normalPath.has_value())
            {
                auto [imgKey, img] = scene.EmplaceImage();
                mat.Normal = imgKey;

                tasks.push_back(ImageTaskData{
                    .ImageKey = imgKey,
                    .Path = normalPath,
                    .BaseColor = Pixel{},
                    .Name = mat.Name + " Normal",
                    .Unorm = true,
                });
            }
        }
    }
}

void GltfAsset::PreprocessMeshes(Scene &scene, std::map<size_t, SceneKey> &meshKeyMap,
                                 std::vector<PrimitiveTaskData> &tasks,
                                 const ModelConfig &config,
                                 const std::map<size_t, SceneKey> &matKeyMap)
{
    vassert(meshKeyMap.empty(), "Key map should be empty!");
    vassert(tasks.empty(), "Tasks vector should be empty!");

    const cgltf_data *gltf = mPImpl->Data;

    const std::string baseName = Path(config.Filepath).Stem();

    // Iterate all gltf meshes:
    for (size_t gltfMeshId = 0; gltfMeshId < gltf->meshes_count; gltfMeshId++)
    {
        const cgltf_mesh &gltfMesh = gltf->meshes[gltfMeshId];

        // Create the new mesh:
        auto [meshKey, mesh] = scene.EmplaceMesh();

        mesh.Name = std::format("{} {}", baseName, SafeName(gltfMesh.name));

        // Update mesh key map:
        meshKeyMap[gltfMeshId] = meshKey;

        // Retrieve its primitives:
        for (size_t gltfPrimId = 0; gltfPrimId < gltfMesh.primitives_count; gltfPrimId++)
        {
            const cgltf_primitive &gltfPrim = gltfMesh.primitives[gltfPrimId];

            // Emplace new primitive:
            auto &newMeshPrim = mesh.Primitives.emplace_back();

            // Assign material keys to the mesh:
            if (gltfPrim.material != nullptr)
            {
                size_t id = cgltf_material_index(gltf, gltfPrim.material);
                newMeshPrim.Material = matKeyMap.at(id);
            }

            tasks.push_back(PrimitiveTaskData{
                .SceneMesh = meshKey,
                .ScenePrim = mesh.Primitives.size() - 1,
                .GltfMesh  = static_cast<int64_t>(gltfMeshId),
                .GltfPrim  = static_cast<int64_t>(gltfPrimId),
            });
        }
    }
}

static auto UnpackTransform(const cgltf_node &node)
    -> std::tuple<glm::vec3, glm::vec3, glm::vec3>
{
    glm::vec3 translation;
    glm::quat quat;
    glm::vec3 scale;

    if (node.has_matrix)
    {
        // NOTE: cgltf matrices are column-major, same as glm:
        glm::mat4 mat;
        std::copy_n(node.matrix, 16, &mat[0][0]);

        translation = glm::vec3(mat[3]);

        glm::vec3 c0(mat[0]), c1(mat[1]), c2(mat[2]);
        scale = glm::vec3(glm::length(c0), glm::length(c1), glm::length(c2));

        // Handle mirroring, so that the remaining basis is a pure rotation:
        if (glm::dot(glm::cross(c0, c1), c2) < 0.0f)
            scale.x = -scale.x;

        glm::mat3 rot;
        rot[0] = c0 / scale.x;
        rot[1] = c1 / scale.y;
        rot[2] = c2 / scale.z;

        quat = glm::quat_cast(rot);
    }
    else
    {
        const auto &t = node.translation;
        const auto &r = node.rotation; // x, y, z, w
        const auto &s = node.scale;

        translation = glm::vec3(t[0], t[1], t[2]);
        quat = glm::quat(r[3], r[0], r[1], r[2]); // glm: w, x, y, z
        scale = glm::vec3(s[0], s[1], s[2]);
    }

    auto rotation = glm::eulerAngles(quat);

    return {translation, rotation, scale};
}

static void ProcessNode(const cgltf_data *gltf,
                        const std::map<size_t, SceneKey> &meshKeyMap,
                        const cgltf_node &current, SceneGraphNode &graphNode)
{
    auto [translation, rotation, scale] = UnpackTransform(current);

    graphNode.Name = SafeName(current.name);
    graphNode.Translation = translation;
    graphNode.Rotation = rotation;
    graphNode.Scale = scale;

    for (size_t i = 0; i < current.children_count; i++)
    {
        const cgltf_node &child = *current.children[i];

        if (child.mesh != nullptr)
        {
            auto meshKey = meshKeyMap.at(cgltf_mesh_index(gltf, child.mesh));

            auto &graphChild = graphNode.EmplaceChild(meshKey);
            ProcessNode(gltf, meshKeyMap, child, graphChild);
        }
        else
        {
            auto &graphChild = graphNode.EmplaceChild();
            ProcessNode(gltf, meshKeyMap, child, graphChild);
        }
    }
}

void GltfAsset::PreprocessHierarchy(SceneGraphNode &root,
                                    const std::map<size_t, SceneKey> &meshKeyMap)
{
    const cgltf_data *gltf = mPImpl->Data;

    vassert(gltf->scenes_count > 0, "Gltf file contains no scenes!");

    // TODO: Currently we assume gltf holds one scene.
    // (cgltf resolves the file's default scene to data->scene,
    // but fall back to the first one like the old implementation.)
    const cgltf_scene &scene = gltf->scene ? *gltf->scene : gltf->scenes[0];

    // NOTE: This will break and result in duplicate
    // objects if the graph of nodes is not acyclic.
    // I have observed such non-cylicity
    // converting obj -> gltf in blender.
    for (size_t i = 0; i < scene.nodes_count; i++)
    {
        const cgltf_node &node = *scene.nodes[i];

        if (node.mesh != nullptr)
        {
            size_t meshIdx = cgltf_mesh_index(gltf, node.mesh);

            if (!meshKeyMap.contains(meshIdx))
            {
                std::cerr << "INVALID MESH KEY: " << meshIdx << "\n";
                continue;
            }

            auto meshKey = meshKeyMap.at(meshIdx);

            auto [translation, rotation, scale] = UnpackTransform(node);

            auto &prefabNode = root.EmplaceChild(meshKey);

            prefabNode.Translation = translation;
            prefabNode.Rotation = rotation;
            prefabNode.Scale = scale;
            prefabNode.Name = SafeName(node.name);
        }
        else
        {
            ProcessNode(gltf, meshKeyMap, node, root);
        }
    }
}

PrimitiveData GltfAsset::LoadPrimitive(PrimitiveTaskData data, const ModelConfig &config)
{
    PrimitiveData res{};

    const cgltf_data *gltf = mPImpl->Data;

    const cgltf_mesh &mesh = gltf->meshes[data.GltfMesh];
    const cgltf_primitive &primitive = mesh.primitives[data.GltfPrim];

    auto flags = GetLoadFlags(config.VertexLayout);

    auto GetPrimitiveInfo = [&]() -> std::string {
        std::string ret = "Gltf file:" + config.Filepath;
        ret += " mesh: " + std::to_string(data.GltfMesh);
        ret += " prim: " + std::to_string(data.GltfPrim);
        ret += " ";
        
        return ret;
    };

    // Retrieve the positions accessor first, since vertex count depends on it:
    const cgltf_accessor *posAccessor =
        cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);

    vassert(posAccessor != nullptr, GetPrimitiveInfo() + "Primitive has no POSITION attribute!");
    vassert(posAccessor->type == cgltf_type_vec3, GetPrimitiveInfo() + "POSITION should be a vec3!");

    res.VertexCount = posAccessor->count;

    // Retrieve indices:
    if (primitive.indices != nullptr)
    {
        res.IndexCount = primitive.indices->count;
        res.Indices.resize(res.IndexCount);

        cgltf_size unpacked = cgltf_accessor_unpack_indices(
            primitive.indices, res.Indices.data(), sizeof(uint32_t), res.IndexCount);

        vassert(unpacked == res.IndexCount, GetPrimitiveInfo() + "Failed to unpack gltf indices!");
    }
    else
    {
        std::cout << GetPrimitiveInfo() << "No indices found. Attempting to use trivial indexing \n";
        
        res.IndexCount = res.VertexCount;
        res.Indices.resize(res.IndexCount);

        for (size_t i = 0; i < res.IndexCount; i++)
        {
            res.Indices[i] = static_cast<uint32_t>(i);
        }
    }

    // Retrieve the positions and calculate the bounding box:
    {
        res.Positions.resize(res.VertexCount);

        cgltf_accessor_unpack_floats(posAccessor,
                                     reinterpret_cast<float *>(res.Positions.data()),
                                     3 * res.VertexCount);

        auto minCoords = glm::vec3(std::numeric_limits<float>::max());
        auto maxCoords = glm::vec3(std::numeric_limits<float>::lowest());

        for (const auto &v : res.Positions)
        {
            minCoords = glm::min(minCoords, v);
            maxCoords = glm::max(maxCoords, v);
        }

        res.BBox.Center = 0.5f * (maxCoords + minCoords);
        res.BBox.Extent = 0.5f * (maxCoords - minCoords);
    }

    // Retrieve texture coords and compute their bounds:
    if (flags.LoadTexCoord)
    {
        const cgltf_accessor *texcoordAccessor =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);

        if (texcoordAccessor != nullptr && texcoordAccessor->type == cgltf_type_vec2)
        {
            res.TexCoords.resize(res.VertexCount);

            if (texcoordAccessor->count != res.VertexCount)
            {
                std::cerr << GetPrimitiveInfo()
                          << "Attribute count discrepancy:"
                          << "num vertices: " << res.VertexCount << ", "
                          << "num texcoords:" << texcoordAccessor->count << '\n';
            }

            const size_t count = std::min<size_t>(texcoordAccessor->count, res.VertexCount);

            cgltf_accessor_unpack_floats(texcoordAccessor,
                                     reinterpret_cast<float *>(res.TexCoords.data()),
                             2 * count);

            auto minCoords = glm::vec2(std::numeric_limits<float>::max());
            auto maxCoords = glm::vec2(std::numeric_limits<float>::lowest());

            for (auto coord : res.TexCoords)
            {
                minCoords = glm::min(minCoords, coord);
                maxCoords = glm::max(maxCoords, coord);
            }

            glm::vec2 center = 0.5f * (maxCoords + minCoords);
            glm::vec2 extent = 0.5f * (maxCoords - minCoords);

            if (extent.x != 0.0 && extent.y != 0.0)
            {
                res.TexBounds.Center = center;
                res.TexBounds.Extent = extent;
            }
            else
            {
                std::cerr << GetPrimitiveInfo()
                          << "degenerate texcoord range: " << minCoords.x << ", "
                          << minCoords.y << "to " << maxCoords.x << ", "
                          << maxCoords.y << '\n';
            }
        }
        else
        {
            std::cerr << GetPrimitiveInfo() << "The primitive doesn't contain texture coordinates.\n";
        }
    }

    // Retrieve normals
    if (flags.LoadNormals)
    {
        const cgltf_accessor *normalAccessor =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);

        if (normalAccessor != nullptr && normalAccessor->type == cgltf_type_vec3)
        {
            const glm::vec3 defaultNormal{0, -1, 0};
            res.Normals.resize(res.VertexCount, defaultNormal);

            if (normalAccessor->count != res.VertexCount)
            {
                std::cerr << GetPrimitiveInfo()
                          << "Attribute count discrepancy:"
                          << "num vertices: " << res.VertexCount << ", "
                          << "num normals:" << normalAccessor->count << '\n';
            }

            const size_t count = std::min<size_t>(normalAccessor->count, res.VertexCount);

            cgltf_accessor_unpack_floats(normalAccessor,
                                     reinterpret_cast<float *>(res.Normals.data()),
                                    3 * count);

            for (auto& v : res.Normals)
            {
                const float tolerance = 0.01f;
                float len = glm::length(v);

                if (len < tolerance)
                {
                    std::cerr << GetPrimitiveInfo()
                              << "Normal vector is degenerate (close to zero): " << v.x
                              << " " << v.y << " " << v.z << '\n';
                    continue;
                }

                if (std::abs(len - 1.0f) > tolerance)
                {
                    std::cerr << GetPrimitiveInfo()
                              << "Provided normal vector is not normalized:" << v.x
                              << " " << v.y << " " << v.z << '\n';
                }

                // Renormalize anyway just to be sure:
                v = glm::normalize(v);
            }
        }
        else
        {
            std::cout << GetPrimitiveInfo() << "primitive doesn't contain normals.\n";
        }
    }

    // Retrieve tangents if present, generate them with mikktspace otherwise
    if (flags.LoadTangents)
    {
        const cgltf_accessor *tangentAccessor =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_tangent, 0);

        if (tangentAccessor != nullptr && tangentAccessor->type == cgltf_type_vec4)
        {
            const glm::vec4 defaultTangent{1, 0, 0, 1};
            res.Tangents.resize(res.VertexCount, defaultTangent);

            if (tangentAccessor->count != res.VertexCount)
            {
                std::cerr << GetPrimitiveInfo()
                          << "Attribute count discrepancy:"
                          << "num vertices: " << res.VertexCount << ", "
                          << "num tangents:" << tangentAccessor->count << '\n';
            }

            const size_t count = std::min<size_t>(tangentAccessor->count, res.VertexCount);

            cgltf_accessor_unpack_floats(tangentAccessor,
                                          reinterpret_cast<float *>(res.Tangents.data()),
                                          4 * count);

            for (auto& v : res.Tangents)
            {
                const float tolerance = 0.01f;
                float len = glm::length(glm::vec3(v));

                if (len < tolerance)
                {
                    std::cerr << GetPrimitiveInfo()
                              << "Tangent vector is degenerate (close to zero): " << v.x
                              << " " << v.y << " " << v.z << '\n';
                    continue;
                }

                if (std::abs(len - 1.0f) > tolerance)
                {
                    std::cerr << GetPrimitiveInfo()
                              << "Provided tangent vector is not normalized:" << v.x
                              << " " << v.y << " " << v.z << '\n';
                }

                v = {glm::normalize(glm::vec3(v)), v.w};
            }
        }
        else
        {
            tangen::GenerateTangents(res);
        }
    }

    // TODO: Fetch colors?

    return res;
}
