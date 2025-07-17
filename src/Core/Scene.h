#pragma once

#include "Bitflags.h"
#include "GeometryData.h"
#include "ImageData.h"

#include <glm/glm.hpp>

#include <map>
#include <mutex>
#include <optional>

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

struct ScenePrimitive {
    GeometryData Data;
    std::optional<SceneKey> Material;
};

struct SceneMesh {
    std::string Name;
    std::vector<ScenePrimitive> Primitives;
};

struct SceneMaterial {
    std::string Name;

    std::optional<SceneKey> Albedo = std::nullopt;
    std::optional<SceneKey> Roughness = std::nullopt;
    std::optional<SceneKey> Normal = std::nullopt;

    float AlphaCutoff = 0.5f;
};

struct SceneObject {
    std::optional<SceneKey> Mesh = std::nullopt;
    glm::mat4 Transform = glm::mat4(1.0f);
};

class Scene {
  public:
    std::map<SceneKey, ImageData> Images;
    std::map<SceneKey, SceneMesh> Meshes;
    std::map<SceneKey, SceneMaterial> Materials;
    std::map<SceneKey, SceneObject> Objects;

    struct Environment {
        bool DirLightOn = true;
        glm::vec3 LightDir = glm::vec3(-0.71f, -0.08f, 0.7f);

        std::optional<SceneKey> HdriImage;
    } Env;

  public:
    std::pair<SceneKey, SceneMesh &> EmplaceMesh();
    std::pair<SceneKey, ImageData &> EmplaceImage();
    std::pair<SceneKey, SceneMaterial &> EmplaceMaterial();
    std::pair<SceneKey, SceneObject &> EmplaceObject();
    std::pair<SceneKey, SceneObject &> EmplaceObject(const SceneObject &existing);

    enum class UpdateFlag
    {
        Images,
        Meshes,
        Materials,
        MeshMaterials,
        Objects,
        Environment,
    };

    void RequestFullReload();
    void RequestUpdateAll();
    void RequestUpdate(UpdateFlag flag);
    void ClearUpdateFlags();

    [[nodiscard]] bool FullReload() const;
    [[nodiscard]] bool UpdateRequested() const;
    [[nodiscard]] bool UpdateImages() const;
    [[nodiscard]] bool UpdateMeshes() const;
    [[nodiscard]] bool UpdateMeshMaterials() const;
    [[nodiscard]] bool UpdateMaterials() const;
    [[nodiscard]] bool UpdateObjects() const;
    [[nodiscard]] bool UpdateEnvironment() const;

  private:
    bool mFullReload = false;
    Bitflags<UpdateFlag> mUpdateFlags;

    SceneKeyGenerator mImageKeyGenerator;
    SceneKeyGenerator mMeshKeyGenerator;
    SceneKeyGenerator mMaterialKeyGenerator;
    SceneKeyGenerator mObjectKeyGenerator;

    std::mutex mMutex;
};