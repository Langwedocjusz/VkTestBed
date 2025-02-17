#pragma once

#include "FilesystemBrowser.h"
#include "ModelLoader.h"
#include "Scene.h"

class ModelLoaderGui {
  public:
    ModelLoaderGui();

    void TriggerLoad();
    void OnImGui(Scene &scene);

  private:
    void ImportMenu(Scene &scene);
    void LoadModel(Scene &scene);

    // Templated, because it handles several types
    // from fastgltf
    template <typename T>
    auto GetTexturePath(fastgltf::Asset &gltf,
                        std::optional<T> &texInfo) -> std::optional<std::filesystem::path>
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
        auto relPath = uri.uri.fspath();

        return mBrowser.CurrentPath / relPath;
    }

  private:
    bool mFilePopup = false;
    bool mImportPopup = false;

    bool mFileMenuOpen = true;
    bool mImportMenuOpen = true;

    ModelLoader::Config mModelConfig;

    struct MaterialConfig {
        bool FetchNormal = true;
        bool FetchRoughness = true;
    } mMatrialConfig;

    FilesystemBrowser mBrowser;
};