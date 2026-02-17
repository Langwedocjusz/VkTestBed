#pragma once

#include "Image.h"
#include "ImageData.h"
#include "Texture.h"
#include "VulkanContext.h"

#include <string>

namespace ImageLoaders
{
//TODO: This api is nonsense, as imagedata now stores information
//about wether or not mips should be generated/loaded etc.
Image LoadImage2D(VulkanContext &ctx, const std::string &debugName,
                  const ImageData &data);

Image LoadImage2DMip(VulkanContext &ctx, const std::string &debugName,
                     const ImageData &data);
} // namespace ImageLoaders

namespace TextureLoaders
{
Texture LoadTexture2D(VulkanContext &ctx, const std::string &debugName,
                      const ImageData &data);

Texture LoadTexture2DMipped(VulkanContext &ctx, const std::string &debugName,
                            const ImageData &data);
} // namespace TextureLoaders