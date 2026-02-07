#pragma once

#include "Image.h"

struct Texture {
    Image       Img;
    VkImageView View;
};