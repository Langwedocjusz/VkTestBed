#pragma once

#include "Image.h"
#include "ImageData.h"
#include "ImageUtils.h"
#include "VulkanContext.h"

#include <array>
#include <string>

namespace ImageLoaders
{
Image LoadImage2D(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                  const ImageData &data, VkFormat format);

Image LoadImage2DMip(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                     const ImageData &data, VkFormat format);
} // namespace ImageLoaders

namespace TextureLoaders
{
Texture LoadTexture2D(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                      const ImageData &data, VkFormat format);

Texture LoadTexture2DMipped(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                            const ImageData &data, VkFormat format);
} // namespace TextureLoaders