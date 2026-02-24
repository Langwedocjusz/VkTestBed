#include "ImageLoaders.h"
#include "Pch.h"

#include "ImageData.h"
#include "ImageUtils.h"
#include "Texture.h"
#include "Vassert.h"

#include "volk.h"

#include <cstdint>

static VkExtent2D GetExtent2D(const ImageData &data)
{
    return VkExtent2D{
        .width  = static_cast<uint32_t>(data.Width),
        .height = static_cast<uint32_t>(data.Height),
    };
}

Image ImageLoaders::LoadImage2D(VulkanContext &ctx, const std::string &debugName,
                                const ImageData &data)
{
    auto extent = GetExtent2D(data);

    Image2DInfo imgInfo{
        .Extent = extent,
        .Format = data.Format,
        .Usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    if (data.Mips == MipStrategy::Load)
        imgInfo.MipLevels = data.NumMips;

    if (data.Mips == MipStrategy::Generate)
    {
        imgInfo.MipLevels = Image::CalcNumMips(data.Width, data.Height);
        imgInfo.Usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    Image img = MakeImage::Image2D(ctx, debugName, imgInfo);

    ImageUploadInfo uploadInfo{
        .Data       = data.Data,
        .Size       = data.Size,
        .DstLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .MipOffsets = {},
    };

    if (data.Mips == MipStrategy::Load)
    {
        uploadInfo.AllMips    = true;
        uploadInfo.MipOffsets = data.MipOffsets;
    }

    Image::UploadToImage(ctx, img, uploadInfo);

    if (data.Mips == MipStrategy::Generate)
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
