#pragma once

#include "FilesystemBrowser.h"
#include "Scene.h"

class ModelLoaderGui {
  public:
    ModelLoaderGui();

    void TriggerLoad();
    void OnImGui(Scene &scene);

  private:
    void FileMenu();
    void ImportMenu(Scene &scene);

    void LoadModel(Scene &scene);

  private:
    bool mFilePopup = false;
    bool mImportPopup = false;

    bool mFileMenuOpen = true;
    bool mImportMenuOpen = true;

    FilesystemBrowser mBrowser;
};