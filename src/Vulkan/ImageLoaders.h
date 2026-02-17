#pragma once

#include "Image.h"
#include "ImageData.h"
#include "Texture.h"
#include "VulkanContext.h"

#include <string>

namespace ImageLoaders
{
Image LoadImage2D(VulkanContext &ctx, const std::string &debugName,
                  const ImageData &data);
} // namespace ImageLoaders

namespace TextureLoaders
{
Texture LoadTexture2D(VulkanContext &ctx, const std::string &debugName,
                      const ImageData &data);
} // namespace TextureLoaders