#include "Scene.h"
#include "Pch.h"

#include "Assert.h"

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
    obj = existing;

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

bool Scene::FullReload() const
{
    return mFullReload;
}

bool Scene::UpdateImages() const
{
    return mUpdateFlags[UpdateFlag::Images];
}

bool Scene::UpdateMeshes() const
{
    return mUpdateFlags[UpdateFlag::Meshes];
}

bool Scene::UpdateMeshMaterials() const
{
    return mUpdateFlags[UpdateFlag::MeshMaterials];
}

bool Scene::UpdateMaterials() const
{
    return mUpdateFlags[UpdateFlag::Materials];
}

bool Scene::UpdateObjects() const
{
    return mUpdateFlags[UpdateFlag::Objects];
}

bool Scene::UpdateEnvironment() const
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