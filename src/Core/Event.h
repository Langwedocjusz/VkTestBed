#pragma once

#include <cstdint>
#include <variant>

namespace Event
{
struct FramebufferResize {
    int32_t Width;
    int32_t Height;
};

struct Focus {
    int32_t Focused;
};

struct CursorEnter {
    int32_t Entered;
};

struct CursorPos {
    double XPos;
    double YPos;
};

struct MouseButton {
    int32_t Button;
    int32_t Action;
    int32_t Mods;
};

struct Scroll {
    double XOffset;
    double YOffset;
};

struct Key {
    int32_t Keycode;
    int32_t Scancode;
    int32_t Action;
    int32_t Mods;
};

struct Char {
    uint32_t Codepoint;
};

// clang-format off
using EventVariant = std::variant<
    FramebufferResize,
    Focus,
    CursorEnter,
    CursorPos,
    MouseButton,
    Scroll,
    Key,
    Char
>;
// clang-format on

}; // namespace Event