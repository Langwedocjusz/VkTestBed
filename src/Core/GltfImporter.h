#pragma once

#include "ModelConfig.h"
#include "Scene.h"
#include "SceneGraph.h"

#include <filesystem>
#include <memory>

struct TextureBounds {
    glm::vec2 Center = glm::vec2(0.5f);
    glm::vec2 Extent = glm::vec2(0.5f);
};

struct PrimitiveData {
    size_t IndexCount;
    size_t VertexCount;
    // TODO: Indices could already be stored as
    // opaque buffer to save that one memcpy
    std::vector<uint32_t>  Indices;
    std::vector<glm::vec3> Positions;
    std::vector<glm::vec2> TexCoords;
    std::vector<glm::vec3> Normals;
    std::vector<glm::vec4> Tangents;
    std::vector<glm::vec4> Colors;
    AABB                   BBox;
    TextureBounds          TexBounds;
};

struct ImageTaskData {
    SceneKey                             ImageKey;
    std::optional<std::filesystem::path> Path;
    Pixel                                BaseColor;
    std::string                          Name;
    bool                                 Unorm;
};

struct PrimitiveTaskData {
    SceneKey SceneMesh;
    size_t   ScenePrim;
    int64_t  GltfMesh;
    int64_t  GltfPrim;
};

class GltfAsset {
  public:
    // Will load gltf buffers into memory:
    GltfAsset(const std::filesystem::path &filepath);
    ~GltfAsset();

    GltfAsset(const GltfAsset &)            = delete;
    GltfAsset &operator=(const GltfAsset &) = delete;

    GltfAsset(GltfAsset &&) noexcept;
    GltfAsset &operator=(GltfAsset &&) noexcept;

    // Retrieves all materials and creates corresponding objects in the scene
    // Fills out a vector of async image-load tasks to be dispatched.
    // Also fills out map (gltf id) -> (scene id) for materials.
    void PreprocessMaterials(Scene &scene, std::map<size_t, SceneKey> &matKeyMap,
                             std::vector<ImageTaskData> &tasks,
                             const ModelConfig          &config);

    // Consumes material table filled by PreprocessMaterials.
    // Retrieves all mesh primitives and creates corresponding objects in the scene
    // Fills out a vector of async primitive-load tasks to be dispatched.
    // Also fills out map (gltf id) -> (scene id) for meshes.
    void PreprocessMeshes(Scene &scene, std::map<size_t, SceneKey> &meshKeyMap,
                          std::vector<PrimitiveTaskData>   &tasks,
                          const ModelConfig                &config,
                          const std::map<size_t, SceneKey> &matKeyMap);

    // Consumes mesh table filled by PreprocessMeshes.
    // Creates a prefab based on the mesh hierarchy of the source gltf.
    void PreprocessHierarchy(SceneGraphNode                   &root,
                             const std::map<size_t, SceneKey> &meshKeyMap);

    // The gltf primitive should be move-returned:
    PrimitiveData LoadPrimitive(PrimitiveTaskData data, const ModelConfig &config);

  private:
    struct Impl;
    std::unique_ptr<Impl> mPImpl;
};