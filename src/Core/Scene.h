#pragma once

#include "Bitflags.h"
#include "GeometryData.h"
#include "ImageData.h"

#include <glm/glm.hpp>

#include <map>
#include <mutex>
#include <optional>

// TODO: replace with a more legit unqiue ID setup:
using SceneKey = uint32_t;

class SceneKeyGenerator {
  public:
    SceneKey Get()
    {
        return mCurrent++;
    }

  private:
    SceneKey mCurrent = 1;
};

struct ScenePrimitive {
    GeometryData            Data;
    std::optional<SceneKey> Material;
    // When using compressed vertex formats its convenient to
    // have vertex positions always be in [0,1] range for quantization.
    // To maintaint object shapes the mapping to original range
    // has to be stored somewhere. Hence when dealing with
    // compressed formats the walues below are used to alter
    // the model matrices of objects.
    glm::vec3 BaseOffset = glm::vec3(0.0f);
    glm::vec3 BaseScale  = glm::vec3(1.0f);
    // For the same reason it's convenient to have texcoords
    // normalized to [0,1] as well. This interferes with
    // having control over texture tiling. As such ranges
    // for the texcoords also have to be stored:
    glm::vec2 TexCoordCenter = glm::vec2(0.5f);
    glm::vec2 TexCoordExtent = glm::vec2(0.5f);
};

struct SceneMesh {
    std::string                 Name;
    std::vector<ScenePrimitive> Primitives;
};

struct SceneMaterial {
    std::string Name;

    std::optional<SceneKey> Albedo    = std::nullopt;
    std::optional<SceneKey> Roughness = std::nullopt;
    std::optional<SceneKey> Normal    = std::nullopt;

    bool  DoubleSided = false;
    float AlphaCutoff = 0.5f;

    std::optional<glm::vec3> TranslucentColor;
};

struct SceneObject {
    std::optional<SceneKey> Mesh      = std::nullopt;
    glm::mat4               Transform = glm::mat4(1.0f);
};

class Scene {
  public:
    std::map<SceneKey, ImageData>     Images;
    std::map<SceneKey, SceneMesh>     Meshes;
    std::map<SceneKey, SceneMaterial> Materials;
    std::map<SceneKey, SceneObject>   Objects;

    AABB TotalAABB;

    struct Environment {
        bool      DirLightOn = true;
        glm::vec3 LightDir   = glm::vec3(-0.71f, -0.08f, 0.7f);
        glm::vec3 LightColor = glm::vec3(1.0f, 1.0f, 0.8f);

        mutable bool             ReloadImage = false;
        std::optional<ImageData> HdriImage;
    } Env;

  public:
    void RecalculateAABB();

    std::pair<SceneKey, SceneMesh &>     EmplaceMesh();
    std::pair<SceneKey, ImageData &>     EmplaceImage();
    std::pair<SceneKey, SceneMaterial &> EmplaceMaterial();
    std::pair<SceneKey, SceneObject &>   EmplaceObject();
    std::pair<SceneKey, SceneObject &>   EmplaceObject(const SceneObject &existing);

    enum class UpdateFlag
    {
        Images,
        Meshes,
        Materials,
        MeshMaterials,
        Objects,
        Environment,
    };

    // Ask for update/reload:
    void RequestFullReload();
    void RequestUpdate(UpdateFlag flag);
    void RequestUpdateAll();
    void ClearUpdateFlags();

    // Check if update/reload is scheduled:
    [[nodiscard]] bool FullReloadRequested() const;
    [[nodiscard]] bool UpdateRequested() const;
    [[nodiscard]] bool UpdateImagesRequested() const;
    [[nodiscard]] bool UpdateMeshesRequested() const;
    [[nodiscard]] bool UpdateMeshMaterialsRequested() const;
    [[nodiscard]] bool UpdateMaterialsRequested() const;
    [[nodiscard]] bool UpdateObjectsRequested() const;
    [[nodiscard]] bool UpdateEnvironmentRequested() const;

  private:
    bool                 mFullReload = false;
    Bitflags<UpdateFlag> mUpdateFlags;

    SceneKeyGenerator mImageKeyGenerator;
    SceneKeyGenerator mMeshKeyGenerator;
    SceneKeyGenerator mMaterialKeyGenerator;
    SceneKeyGenerator mObjectKeyGenerator;

    std::mutex mMutex;
};