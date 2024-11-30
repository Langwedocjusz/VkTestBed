#pragma once

#include "Scene.h"

class SceneEditor {
  public:
    void OnImGui(Scene &scene);

  private:
    void SceneHierarchyMenu(Scene &scene);

    void DataMenu(Scene &scene);
    void AddProviderPopup([[maybe_unused]] Scene &scene);

    void InstanceMenu(SceneGraphNode &node, SceneGraphNode *parent = nullptr,
                      int64_t childId = 0);
    void AddInstancePopup(Scene &scene);

    struct DragPayload {
        SceneGraphNode *Parent;
        int64_t ChildId;
    };

    void HandleSceneDropPayload(SceneGraphNode &node);

    void ObjectPropertiesMenu(Scene &scene);

    void HandleNodeOp(Scene &scene);
    void HandleNodeMove();
    void HandleNodeDelete(Scene &scene);
    void HandleNodeCopy(Scene &scene);

  private:
    SceneGraphNode *mSelectedNode = nullptr;

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
};