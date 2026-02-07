#include "Scene.h"
#include "GeometryData.h"
#include "Pch.h"

#include "Vassert.h"

void Scene::RecalculateAABB()
{
    bool firstIteration = true;
    AABB bbox{};

    for (auto &[_, obj] : Objects)
    {
        if (!obj.Mesh.has_value())
            continue;

        auto &mesh      = Meshes[*obj.Mesh];
        auto &transform = obj.Transform;

        for (auto &prim : mesh.Primitives)
        {
            auto srcBBox         = prim.Data.BBox;
            auto transformedBBox = srcBBox.GetConservativeTransformedAABB(transform);

            if (firstIteration)
            {
                bbox           = transformedBBox;
                firstIteration = false;
            }

            else
            {
                bbox = bbox.MaxWith(transformedBBox);
            }
        }
    }

    TotalAABB = bbox;
}

std::pair<SceneKey, SceneMesh &> Scene::EmplaceMesh()
{
    std::unique_lock lock(mMutex);

    auto key = mMeshKeyGenerator.Get();
    vassert(Meshes.count(key) == 0);

    Meshes[key] = SceneMesh{};

    return {key, Meshes[key]};
}

std::pair<SceneKey, ImageData &> Scene::EmplaceImage()
{
    std::unique_lock lock(mMutex);

    auto key = mImageKeyGenerator.Get();
    vassert(Images.count(key) == 0);

    Images[key] = ImageData{};

    return {key, Images[key]};
}

std::pair<SceneKey, SceneMaterial &> Scene::EmplaceMaterial()
{
    std::unique_lock lock(mMutex);

    auto key = mMaterialKeyGenerator.Get();
    vassert(Materials.count(key) == 0);

    Materials[key] = SceneMaterial{};

    return {key, Materials[key]};
}

std::pair<SceneKey, SceneObject &> Scene::EmplaceObject()
{
    std::unique_lock lock(mMutex);

    auto key = mObjectKeyGenerator.Get();
    vassert(Objects.count(key) == 0);

    Objects[key] = SceneObject{};

    return {key, Objects[key]};
}

std::pair<SceneKey, SceneObject &> Scene::EmplaceObject(const SceneObject &existing)
{
    auto [key, obj] = EmplaceObject();
    obj             = existing;

    return {key, obj};
}

void Scene::RequestFullReload()
{
    mFullReload = true;
    RequestUpdateAll();
}

void Scene::RequestUpdateAll()
{
    mUpdateFlags.SetAll();
}

void Scene::RequestUpdate(UpdateFlag flag)
{
    mUpdateFlags.Set(flag);
}

bool Scene::FullReloadRequested() const
{
    return mFullReload;
}

bool Scene::UpdateImagesRequested() const
{
    return mUpdateFlags[UpdateFlag::Images];
}

bool Scene::UpdateMeshesRequested() const
{
    return mUpdateFlags[UpdateFlag::Meshes];
}

bool Scene::UpdateMeshMaterialsRequested() const
{
    return mUpdateFlags[UpdateFlag::MeshMaterials];
}

bool Scene::UpdateMaterialsRequested() const
{
    return mUpdateFlags[UpdateFlag::Materials];
}

bool Scene::UpdateObjectsRequested() const
{
    return mUpdateFlags[UpdateFlag::Objects];
}

bool Scene::UpdateEnvironmentRequested() const
{
    return mUpdateFlags[UpdateFlag::Environment];
}

bool Scene::UpdateRequested() const
{
    return mUpdateFlags.Any();
}

void Scene::ClearUpdateFlags()
{
    mFullReload = false;
    mUpdateFlags.Clear();
}