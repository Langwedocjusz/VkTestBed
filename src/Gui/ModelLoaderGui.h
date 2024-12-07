#pragma once

#include "FilesystemBrowser.h"
#include "Scene.h"

class ModelLoaderGui {
  public:
    void TriggerLoad(Scene &scene);
    void OnImGui();

  private:
    void FileMenu();
    void ImportMenu();

    void LoadModel();

  private:
    bool mFilePopup = false;
    bool mImportPopup = false;

    bool mFileMenuOpen = true;
    bool mImportMenuOpen = true;

    Scene *mScene = nullptr;

    FilesystemBrowser mBrowser;
};