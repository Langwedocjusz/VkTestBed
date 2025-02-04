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

static uint32_t CalcNumMips(int width, int height)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

static Image LoadImage2DImpl(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                             const std::string &path, VkFormat format, bool allocMips)
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

    uint32_t mipLevels = allocMips ? CalcNumMips(texWidth, texHeight) : 1;

    uint32_t usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (allocMips)
        usage = usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    ImageInfo img_info{
        .Extent = extent,
        .Format = format,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = usage,
        .MipLevels = mipLevels,
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
    Image::GenerateMips(ctx, queue, pool, img);

    stbi_image_free(pixels);

    return img;
}

Image ImageLoaders::LoadImage2D(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                                const std::string &path, VkFormat format)
{
    return LoadImage2DImpl(ctx, queue, pool, path, format, false);
}

Image ImageLoaders::LoadImage2DMip(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                                   const std::string &path, VkFormat format)
{
    return LoadImage2DImpl(ctx, queue, pool, path, format, true);
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

    ImageInfo img_info{
        .Extent = extent,
        .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = 1,
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
        .MipLevels = 1,
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