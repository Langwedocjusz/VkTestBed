#pragma once

#include <cstdint>
#include <variant>

namespace Event
{
struct KeyPressed {
    int32_t Keycode;
    bool Repeat;
};

struct KeyReleased {
    int32_t Keycode;
};

struct MouseMoved {
    float X;
    float Y;
};

struct MousePressed {
    int32_t Button;
    int32_t Mods;
};

struct MouseReleased {
    int32_t Button;
    int32_t Mods;
};

using EventVariant = std::variant<KeyPressed, KeyReleased, MouseMoved
                                  // MousePressed,
                                  // MouseReleased
                                  >;
}; // namespace Event