#pragma once

#include "Image.h"
#include "VulkanContext.h"

#include <array>
#include <string>

namespace ImageLoaders
{
Image LoadImage2D(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                  const std::string &path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

Image LoadHDRI(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
               const std::string &path);

// To-do: implement conversion from equirectangular to cubemap:
// std::array<Image, 6> LoadHDRIToCubemap(VulkanContext &ctx, VkQueue queue,
//                                        VkCommandPool pool, const std::string &path);

struct Pixel {
    uint8_t R;
    uint8_t G;
    uint8_t B;
    uint8_t A;
};

struct Image2DData {
    uint32_t Width;
    uint32_t Height;

    std::vector<Pixel> Data;
};

Image Image2DFromData(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                      const Image2DData &data, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
} // namespace ImageLoaders