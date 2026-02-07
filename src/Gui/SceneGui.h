#pragma once

#include "Camera.h"
#include "ModelLoaderGui.h"
#include "SceneEditor.h"

enum class GizmoMode
{
    Translate,
    Rotate,
    Scale,
};

class SceneGui {
  public:
    SceneGui(SceneEditor &editor, const Camera &camera);

    void OnImGui();
    void OnEvent(Event::EventVariant event);

    void                                  SetSelection(SceneKey objKey);
    [[nodiscard]] std::optional<SceneKey> GetSelection() const;

  private:
    void SceneHierarchyMenu();

    void InstanceGui(SceneGraphNode &node, SceneGraphNode *parent = nullptr,
                     int64_t childId = 0);
    void AddInstancePopup();
    void HandleSceneDropPayload(SceneGraphNode &node);

    void DataMenu();

    void MeshesTab();
    void MaterialsTab();
    void ImagesTab();
    void EnvironmentTab();

    void AddProviderPopup();

    void ObjectPropertiesMenu();

    std::string GetMaterialName(std::optional<SceneKey> key, std::string_view postfix);

  private:
    SceneEditor  &mEditor;
    const Camera &mCamera;

    SceneGraphNode *mSelectedNode = nullptr;

    struct DragPayload {
        SceneGraphNode *Parent;
        int64_t         ChildId;
    };

    GizmoMode mGizmoMode = GizmoMode::Translate;

    bool              mOpenHdriPopup = false;
    bool              mHdriStillOpen = true;
    FilesystemBrowser mHdriBrowser;

    ModelLoaderGui mModelLoader;
};