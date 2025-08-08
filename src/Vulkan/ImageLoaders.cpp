#include "ImageLoaders.h"
#include "Pch.h"

#include "ImageData.h"
#include "ImageUtils.h"
#include "Texture.h"
#include "Vassert.h"

#include <utility>
#include <vulkan/vulkan.h>

#include <cstdint>

static VkDeviceSize BytesPerPixel(VkFormat format)
{
    // To-do: handle all formats

    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_SRGB:
        return 4;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return 4;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 16;
    default:
        vpanic("Unsupported or invalid format!");
    }

    std::unreachable();
}

static std::tuple<VkDeviceSize, VkExtent2D> RepackImgData(const ImageData &data)
{
    VkDeviceSize size = data.Width * data.Height * BytesPerPixel(data.Format);

    VkExtent2D extent{static_cast<uint32_t>(data.Width),
                      static_cast<uint32_t>(data.Height)};

    return {size, extent};
}

Image ImageLoaders::LoadImage2D(VulkanContext &ctx, const std::string &debugName,
                                const ImageData &data)
{
    auto [imageSize, extent] = RepackImgData(data);

    Image2DInfo imgInfo{
        .Extent = extent,
        .Format = data.Format,
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
                                   const ImageData &data)
{
    auto [imageSize, extent] = RepackImgData(data);

    Image2DInfo imgInfo{
        .Extent = extent,
        .Format = data.Format,
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
                                      const ImageData &data)
{
    Texture res{};

    res.Img = ImageLoaders::LoadImage2D(ctx, debugName, data);
    res.View =
        MakeView::View2D(ctx, debugName, res.Img, data.Format, VK_IMAGE_ASPECT_COLOR_BIT);

    return res;
}

Texture TextureLoaders::LoadTexture2DMipped(VulkanContext &ctx,
                                            const std::string &debugName,
                                            const ImageData &data)
{
    Texture res{};

    res.Img = ImageLoaders::LoadImage2DMip(ctx, debugName, data);
    res.View =
        MakeView::View2D(ctx, debugName, res.Img, data.Format, VK_IMAGE_ASPECT_COLOR_BIT);

    return res;
}