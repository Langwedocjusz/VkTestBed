#include "Pch.h"
#include "ImageLoaders.h"

#include "ImageData.h"
#include "ImageUtils.h"
#include "Texture.h"

#include <vulkan/vulkan.h>

#include <cstdint>

static std::tuple<VkDeviceSize, VkExtent2D> RepackImgData(const ImageData &data)
{
    VkDeviceSize size = data.Width * data.Height * data.Channels * data.BytesPerChannel;

    VkExtent2D extent{static_cast<uint32_t>(data.Width),
                      static_cast<uint32_t>(data.Height)};

    return {size, extent};
}

Image ImageLoaders::LoadImage2D(VulkanContext &ctx, const std::string &debugName,
                                const ImageData &data, VkFormat format)
{
    auto [imageSize, extent] = RepackImgData(data);

    Image2DInfo imgInfo{
        .Extent = extent,
        .Format = format,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = 1,
    };

    Image img = MakeImage::Image2D(ctx, debugName, imgInfo);

    ImageUploadInfo uploadInfo{
        .Data = data.Data,
        .Size = imageSize,
        .DstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    Image::UploadToImage(ctx, img, uploadInfo);

    return img;
}

Image ImageLoaders::LoadImage2DMip(VulkanContext &ctx, const std::string &debugName,
                                   const ImageData &data, VkFormat format)
{
    auto [imageSize, extent] = RepackImgData(data);

    Image2DInfo imgInfo{
        .Extent = extent,
        .Format = format,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = Image::CalcNumMips(data.Width, data.Height),
    };

    Image img = MakeImage::Image2D(ctx, debugName, imgInfo);

    ImageUploadInfo uploadInfo{
        .Data = data.Data,
        .Size = imageSize,
        .DstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    Image::UploadToImage(ctx, img, uploadInfo);
    Image::GenerateMips(ctx, img);

    return img;
}

Texture TextureLoaders::LoadTexture2D(VulkanContext &ctx, const std::string &debugName,
                                      const ImageData &data, VkFormat format)
{
    Texture res{};

    res.Img = ImageLoaders::LoadImage2D(ctx, debugName, data, format);
    res.View =
        MakeView::View2D(ctx, debugName, res.Img, format, VK_IMAGE_ASPECT_COLOR_BIT);

    return res;
}

Texture TextureLoaders::LoadTexture2DMipped(VulkanContext &ctx,
                                            const std::string &debugName,
                                            const ImageData &data, VkFormat format)
{
    Texture res{};

    res.Img = ImageLoaders::LoadImage2DMip(ctx, debugName, data, format);
    res.View =
        MakeView::View2D(ctx, debugName, res.Img, format, VK_IMAGE_ASPECT_COLOR_BIT);

    return res;
}