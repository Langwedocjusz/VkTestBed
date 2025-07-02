#pragma once

#include "ModelLoaderGui.h"
#include "SceneEditor.h"

class SceneGui {
  public:
    SceneGui(SceneEditor &editor);

    void OnImGui();

  private:
    void SceneHierarchyMenu();

    void InstanceGui(SceneGraphNode &node, SceneGraphNode *parent = nullptr,
                     int64_t childId = 0);
    void AddInstancePopup();
    void HandleSceneDropPayload(SceneGraphNode &node);

    void DataMenu();

    void MeshesTab();
    void MaterialsTab();
    void EnvironmentTab();
    void AddProviderPopup();

    void ObjectPropertiesMenu();

    std::string GetMaterialName(std::optional<SceneKey> key, std::string_view postfix);

  private:
    SceneEditor &mEditor;
    SceneGraphNode *mSelectedNode = nullptr;

    struct DragPayload {
        SceneGraphNode *Parent;
        int64_t ChildId;
    };

    bool mOpenHdriPopup = false;
    bool mHdriStillOpen = true;
    FilesystemBrowser mHdriBrowser;

    ModelLoaderGui mModelLoader;
};