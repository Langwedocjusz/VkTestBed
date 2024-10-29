#pragma once

#include "Image.h"

#include <string>

namespace ImageLoaders
{
Image LoadImage2D(VulkanContext &ctx, VkQueue queue, VkCommandPool pool, const std::string& path);
}