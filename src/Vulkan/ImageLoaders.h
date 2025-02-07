#pragma once

#include "Image.h"
#include "ImageData.h"
#include "VulkanContext.h"

#include <array>
#include <string>

namespace ImageLoaders
{
Image LoadImage2D(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                      ImageData *data, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

Image LoadImage2DMip(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                      ImageData *data, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
} // namespace ImageLoaders