#pragma once

#include "Frame.h"
#include "imgui.h"

#include <string>

namespace imutils
{
bool CloseButton(const char *name, ImVec2 pos);
void DisplayStats(FrameStats &stats);

struct NodeDeletableState {
    bool IsOpen    = false;
    bool IsDeleted = false;
};

struct NodeCopyDeletableState {
    bool IsOpen    = false;
    bool IsDeleted = false;
    bool IsCopied  = false;
    bool IsClicked = false;
};

NodeDeletableState TreeNodeExDeletable(const char *name, ImGuiTreeNodeFlags flags = 0);
NodeCopyDeletableState TreeNodeExDeleteCopyAble(std::string       &name,
                                                ImGuiTreeNodeFlags flags = 0);
} // namespace imutils