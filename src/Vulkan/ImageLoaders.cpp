#include "ImageLoaders.h"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <cstdint>
#include <iostream>

static std::tuple<VkDeviceSize, VkExtent3D> RepackImgData(int width, int height,
                                                          int channels,
                                                          int bytesPerChannel = 1)
{
    VkDeviceSize size = width * height * channels * bytesPerChannel;
    VkExtent3D extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

    return {size, extent};
}

Image ImageLoaders::LoadImage2D(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                                const std::string &path, VkFormat format)
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

    auto [imageSize, extent] = RepackImgData(texWidth, texHeight, 4);

    ImageInfo img_info{
        .Extent = extent,
        .Format = format,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    Image img = Image::CreateImage2D(ctx, img_info);

    ImageUploadInfo data_info{
        .Queue = queue,
        .Pool = pool,
        .Data = pixels,
        .Size = imageSize,
        .DstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    Image::UploadToImage(ctx, img, data_info);

    stbi_image_free(pixels);

    return img;
}

static float *LoadImageEXR(int &width, int &height, const std::string &path)
{
    float *data;
    const char *err = nullptr;

    int ret = LoadEXR(&data, &width, &height, path.c_str(), &err);

    if (ret != TINYEXR_SUCCESS)
    {
        std::string msg = "Error when trying to open: " + path + "\n";

        if (err)
        {
            msg += std::string(err);
        }

        throw std::runtime_error(msg);
    }

    return data;
}

Image ImageLoaders::LoadHDRI(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                             const std::string &path)
{
    // Retrieve data from disk:
    int width, height;
    float *data = LoadImageEXR(width, height, path);

    // Create vulkan image and upload data:
    auto [imageSize, extent] = RepackImgData(width, height, 4, 4);

    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

    ImageInfo img_info{
        .Extent = extent,
        .Format = format,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    Image img = Image::CreateImage2D(ctx, img_info);

    ImageUploadInfo data_info{
        .Queue = queue,
        .Pool = pool,
        .Data = data,
        .Size = imageSize,
        .DstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    Image::UploadToImage(ctx, img, data_info);

    // Free cpu side data:
    free(data);

    return img;
}

Image ImageLoaders::Image2DFromData(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                                    const Image2DData &data, VkFormat format)
{
    assert(data.Data.size() == data.Width * data.Height);

    auto [imageSize, extent] = RepackImgData(data.Width, data.Height, 4);

    ImageInfo img_info{
        .Extent = extent,
        .Format = format,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    Image img = Image::CreateImage2D(ctx, img_info);

    ImageUploadInfo data_info{.Queue = queue,
                              .Pool = pool,
                              .Data = data.Data.data(),
                              .Size = imageSize,
                              .DstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    Image::UploadToImage(ctx, img, data_info);

    return img;
}