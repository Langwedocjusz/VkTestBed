#pragma once

#include "ModelConfig.h"
#include "Scene.h"

#include <filesystem>
#include <memory>

struct GltfPrimitive {
    size_t                 IndexCount;
    size_t                 VertexCount;
    // TODO: Indices could already be stored as 
    // opaque buffer to save that one memcpy
    std::vector<uint32_t>  Indices;
    std::vector<glm::vec3> Positions;
    std::vector<glm::vec2> TexCoords;
    std::vector<glm::vec3> Normals;
    std::vector<glm::vec4> Tangents;
    std::vector<glm::vec4> Colors;
    AABB                   BBox;
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
    GltfAsset()  = default;
    ~GltfAsset() = default;

    static GltfAsset LoadGltfWithBuffers(const std::filesystem::path &filepath);

    void PreprocessMaterials(Scene &scene, std::map<size_t, SceneKey> keyMap,
                             std::vector<ImageTaskData> &tasks,
                             const ModelConfig          &config);
    void PreprocessMeshes(Scene &scene, std::map<size_t, SceneKey> keyMap,
                          std::vector<PrimitiveTaskData> &tasks,
                          const ModelConfig              &config);

    //The gltf primitive should be move-returned:
    GltfPrimitive LoadPrimitive(PrimitiveTaskData data, const ModelConfig &config);

  private:
    struct Impl;
    std::unique_ptr<Impl> mPImpl;
};