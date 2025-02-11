#pragma once

#include "FilesystemBrowser.h"
#include "ModelLoaderGui.h"
#include "Scene.h"

class SceneEditor {
  public:
    void OnInit(Scene &scene);
    void OnImGui(Scene &scene);

  private:
    void SceneHierarchyMenu(Scene &scene);

    void InstanceGui(SceneGraphNode &node, SceneGraphNode *parent = nullptr,
                     int64_t childId = 0);
    void AddInstancePopup(Scene &scene);
    void HandleSceneDropPayload(SceneGraphNode &node);
    void HandleNodeOp(Scene &scene);
    void HandleNodeMove();
    void HandleNodeDelete(Scene &scene);
    void HandleNodeCopy(Scene &scene);

    void DataMenu(Scene &scene);

    void MeshesTab(Scene &scene);
    void MaterialsTab(Scene &scene);
    void EnvironmentTab(Scene &scene);
    void AddProviderPopup([[maybe_unused]] Scene &scene);
    void SelectHdriPopup();

    void ObjectPropertiesMenu(Scene &scene);

  private:
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
    ModelLoaderGui mModelLoader;

    bool mOpenHdriPopup = false;
    bool mHdriStillOpen = true;
    FilesystemBrowser mHdriBrowser;

    SceneGraphNode *mSelectedNode = nullptr;
};