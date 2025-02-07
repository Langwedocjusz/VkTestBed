#include "ImageLoaders.h"

#include "ImageData.h"

#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include <cmath>
#include <cstdint>
#include <iostream>

static std::tuple<VkDeviceSize, VkExtent3D> RepackImgData(ImageData &data)
{
    VkDeviceSize size = data.Width * data.Height * data.Channels * data.BytesPerChannel;
    VkExtent3D extent{static_cast<uint32_t>(data.Width),
                      static_cast<uint32_t>(data.Height), 1};

    return {size, extent};
}

static uint32_t CalcNumMips(int width, int height)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

Image ImageLoaders::LoadImage2D(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                                    ImageData *data, VkFormat format)
{
    auto [imageSize, extent] = RepackImgData(*data);

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
                              .Data = data->Data,
                              .Size = imageSize,
                              .DstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    Image::UploadToImage(ctx, img, data_info);

    return img;
}

Image ImageLoaders::LoadImage2DMip(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                                    ImageData *data, VkFormat format)
{
    auto [imageSize, extent] = RepackImgData(*data);

    ImageInfo img_info{
        .Extent = extent,
        .Format = format,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = CalcNumMips(data->Width, data->Height),
    };

    Image img = Image::CreateImage2D(ctx, img_info);

    ImageUploadInfo data_info{.Queue = queue,
                              .Pool = pool,
                              .Data = data->Data,
                              .Size = imageSize,
                              .DstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    Image::UploadToImage(ctx, img, data_info);
    Image::GenerateMips(ctx, queue, pool, img);

    return img;
}