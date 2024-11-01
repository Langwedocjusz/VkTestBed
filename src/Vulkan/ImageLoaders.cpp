#include "ImageLoaders.h"
#include <cstdint>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Image ImageLoaders::LoadImage2D(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                                const std::string &path)
{
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels =
        stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels)
    {
        std::string err_msg = "Failed to load texture image!\n";
        err_msg += "Filepath: " + path;

        throw std::runtime_error(err_msg);
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkExtent3D extent{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight),
                      1};

    ImageInfo img_info{
        .Extent = extent,
        .Format = VK_FORMAT_R8G8B8A8_SRGB,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    Image img = Image::CreateImage2D(ctx, img_info);

    ImageUploadInfo data_info{.Queue = queue,
                              .Pool = pool,
                              .Data = pixels,
                              .Size = imageSize,
                              .DstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    Image::UploadToImage(ctx, img, data_info);

    stbi_image_free(pixels);

    return img;
}