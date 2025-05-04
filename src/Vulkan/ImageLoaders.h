#pragma once

#include "Image.h"
#include "ImageData.h"
#include "Texture.h"
#include "VulkanContext.h"

#include <string>

namespace ImageLoaders
{
Image LoadImage2D(VulkanContext &ctx, const std::string &debugName, const ImageData &data,
                  VkFormat format);

Image LoadImage2DMip(VulkanContext &ctx, const std::string &debugName,
                     const ImageData &data, VkFormat format);
} // namespace ImageLoaders

namespace TextureLoaders
{
Texture LoadTexture2D(VulkanContext &ctx, const std::string &debugName,
                      const ImageData &data, VkFormat format);

Texture LoadTexture2DMipped(VulkanContext &ctx, const std::string &debugName,
                            const ImageData &data, VkFormat format);
} // namespace TextureLoaders