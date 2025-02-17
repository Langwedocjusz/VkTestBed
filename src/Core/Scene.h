#pragma once

#include "Bitflags.h"
#include "GeometryData.h"
#include "Material.h"
#include "ModelLoader.h"

#include <glm/glm.hpp>

#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <string>

// To-do: replace with a more legit unqiue ID setup:
using SceneKey = uint32_t;

class SceneKeyGenerator {
  public:
    SceneKey Get()
    {
        return mCurrent++;
    }

  private:
    SceneKey mCurrent = 0;
};

struct SceneMesh {
    std::string Name;
    std::variant<ModelLoader::Config, std::function<GeometryData()>> Geometry;
    std::vector<SceneKey> Materials;

    [[nodiscard]] bool IsModel() const;
    [[nodiscard]] const ModelLoader::Config &GetModelConfig() const;
    [[nodiscard]] GeometryData GetGeometry() const;
};

struct SceneObject {
    std::optional<SceneKey> MeshId;

    glm::mat4 Transform;
};

class Scene;

class SceneGraphNode {
  public:
    using ChildrenArray = std::vector<std::unique_ptr<SceneGraphNode>>;

  public:
    SceneGraphNode();
    SceneGraphNode(SceneKey key);

    bool IsLeaf();

    SceneKey GetObjectKey();
    ChildrenArray &GetChildren();

    /// Emplaces a non-leaf node as child
    SceneGraphNode &EmplaceChild();
    /// Emplaces a leaf node as child
    SceneGraphNode &EmplaceChild(SceneKey key);

    glm::mat4 GetTransform();
    void UpdateTransforms(Scene &scene, glm::mat4 current = 1.0f);

  public:
    SceneGraphNode *Parent = nullptr;

    glm::vec3 Translation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};

    std::string Name;

  private:
    std::variant<ChildrenArray, SceneKey> Payload;
};

class Scene {
  public:
    /// Emplace new things, return reference to them:
    SceneMesh &EmplaceMesh();
    Material &EmplaceMaterial();
    SceneObject &EmplaceObject();

    /// Emplace new things, return their key and reference to them:
    std::pair<SceneKey, Material &> EmplaceMaterial2();
    std::pair<SceneKey, SceneObject &> EmplaceObject2();

    /// Emplace existing things, return their key:
    SceneKey EmplaceObject(const SceneObject &obj);

    /// Erase things:
    void EraseMesh(SceneKey key);
    void EraseMaterial(SceneKey key);
    void EraseObject(SceneKey key);

    /// Retrieve singular things:
    SceneMesh &GetMesh(SceneKey key);
    Material &GetMaterial(SceneKey key);
    SceneObject &GetObject(SceneKey key);

    /// Get iterable ranges to things:
    auto Meshes()
    {
        return std::views::all(mMeshes);
    }
    auto Materials()
    {
        return std::views::all(mMaterials);
    }
    auto Objects()
    {
        return std::views::all(mObjects);
    }

    // Api to manage update flags, to keep in sync with renderers:
    bool UpdateRequested();
    void ClearUpdateFlags();

    void RequestFullUpdate();
    void RequestMeshUpdate();
    void RequestMaterialUpdate();
    void RequestMeshMaterialUpdate();
    void RequestObjectUpdate();
    void RequestEnvironmentUpdate();

    bool UpdateMeshes();
    bool UpdateMaterials();
    bool UpdateMeshMaterials();
    bool UpdateObjects();
    bool UpdateEnvironment();

  public:
    // Root of the scene-graph used by UI to control objects:
    SceneGraphNode GraphRoot;

    // Environment configuration:
    struct Environment {
        bool DirLightOn = true;
        glm::vec3 LightDir = glm::vec3(-0.71f, -0.08f, 0.7f);

        std::optional<std::filesystem::path> HdriPath;
    } Env;

  private:
    // Data sources for renderers:
    std::map<SceneKey, SceneMesh> mMeshes;
    std::map<SceneKey, Material> mMaterials;
    std::map<SceneKey, SceneObject> mObjects;

    SceneKeyGenerator mMeshKeyGenerator;
    SceneKeyGenerator mMaterialKeyGenerator;
    SceneKeyGenerator mObjectKeyGenerator;

    // Update data:
    enum class Update
    {
        Materials,
        MeshGeo,
        MeshMaterials,
        Instances,
        Environment,
    };

    Bitflags<Update> mUpdateFlags;
};