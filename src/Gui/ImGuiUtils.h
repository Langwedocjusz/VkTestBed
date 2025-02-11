#pragma once

#include "Frame.h"
#include "imgui.h"

#include <string>

namespace imutils
{
bool CloseButton(const std::string &name, ImVec2 pos);
void DisplayStats(FrameStats &stats);
} // namespace imutils