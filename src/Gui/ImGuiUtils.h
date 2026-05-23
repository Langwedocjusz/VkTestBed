#pragma once

#include "Frame.h"
#include "imgui.h"

#include <span>
#include <string>

namespace imutils
{
bool CloseButton(const char *name, ImVec2 pos);

bool Combo(const char *name, int32_t &choice, std::span<const char *> options);

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

void DisplayStats(FrameStats &stats);
} // namespace imutils