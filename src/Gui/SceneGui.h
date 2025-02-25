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
    void HandleNodeOp();
    void HandleNodeMove();
    void HandleNodeDelete();
    void HandleNodeCopy();

    void DataMenu();

    void MeshesTab();
    void MaterialsTab();
    void EnvironmentTab();
    void AddProviderPopup();
    void SelectHdriPopup();

    void ObjectPropertiesMenu();

  private:
    SceneEditor &mEditor;
    SceneGraphNode *mSelectedNode = nullptr;

    struct DragPayload {
        SceneGraphNode *Parent;
        int64_t ChildId;
    };

    enum class OpType
    {
        None,
        Move,
        Delete,
        Copy
    };

    struct OpInfo {
        OpType RequestedOp = OpType::None;
        SceneGraphNode *Parent;
        int64_t ChildId;
        SceneGraphNode *DstParent;

        SceneGraphNode &GetSourceNode();
        auto GetSourceNodeIterator();
    };

    OpInfo mOpInfo;

    bool mOpenHdriPopup = false;
    bool mHdriStillOpen = true;
    FilesystemBrowser mHdriBrowser;

    ModelLoaderGui mModelLoader;
};