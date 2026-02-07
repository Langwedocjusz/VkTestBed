#pragma once

#include "FilesystemBrowser.h"
#include "ModelConfig.h"
#include "SceneEditor.h"

class ModelLoaderGui {
  public:
    ModelLoaderGui(SceneEditor &editor);

    void TriggerLoad();
    void OnImGui();

  private:
    void ImportMenu();
    void LoadModel();

  private:
    SceneEditor &mEditor;

    bool mFilePopup   = false;
    bool mImportPopup = false;

    bool mFileMenuOpen   = true;
    bool mImportMenuOpen = true;

    ModelConfig mModelConfig;

    FilesystemBrowser mBrowser;
};