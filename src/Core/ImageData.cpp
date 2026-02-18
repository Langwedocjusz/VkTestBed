#include "ImageData.h"
#include "Pch.h"

#include "Vassert.h"

#define KHRONOS_STATIC
#include "ktx.h"
#include "ktxvulkan.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <filesystem>
#include <utility>

static VkDeviceSize BytesPerPixel(VkFormat format)
{
    // TODO: handle all formats
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_SRGB:
        return 4;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return 4;
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return 1;
    case VK_FORMAT_BC7_UNORM_BLOCK:
        return 1;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 16;
    default:
        vpanic("Unsupported or invalid format!");
    }

    std::unreachable();
}

ImageData ImageData::SinglePixel(Pixel p, bool unorm)
{
    auto data = new Pixel(p);

    auto res = ImageData();

    res.Name   = "SinglePixel";
    res.Width  = 1;
    res.Height = 1;
    res.Mips   = MipStrategy::DoNothing;
    res.Format = unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
    res.Data   = static_cast<void *>(data);
    res.Size   = BytesPerPixel(res.Format);
    res.mType  = Type::Pixel;

    return res;
}

ImageData ImageData::ImportImage(const char *path, bool unorm)
{
    std::filesystem::path pathObj(path);

    auto res = ImageData();
    res.Name = pathObj.stem().string();

    if (pathObj.extension().string() == ".ktx" || pathObj.extension().string() == ".ktx2")
    {
        ktxTexture *texture; // TODO: This needs to be stored as well

        auto result = ktxTexture_CreateFromNamedFile(
            path, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
        vassert(result == KTX_SUCCESS);

        // Retrieve info about the texture:
        ktx_uint32_t baseWidth  = texture->baseWidth;
        ktx_uint32_t baseHeight = texture->baseHeight;

        ktx_size_t dataSize = ktxTexture_GetDataSize(texture);

        // TODO: Support more image types
        // ktx_uint32_t baseDepth = texture->baseDepth;
        // ktx_uint32_t numLevels = texture->numLevels;
        // ktx_bool_t isArray = texture->isArray;

        auto mips = texture->generateMipmaps ? MipStrategy::Generate : MipStrategy::Load;

        if (mips == MipStrategy::Load)
        {
            res.NumMips = texture->numLevels;

            for (size_t lvl=0; lvl<res.NumMips; lvl++)
            {
                ktx_size_t offset{0};
                auto ret = ktxTexture_GetImageOffset(texture, lvl, 0, 0, &offset);

                vassert(ret == KTX_SUCCESS);

                res.MipOffsets.push_back(offset);
            }
        }

        ktx_uint8_t *image = ktxTexture_GetData(texture);

        VkFormat format = ktxTexture_GetVkFormat(texture);

        // TODO: this is a horrible hack:
        if (unorm && (format == VK_FORMAT_BC7_SRGB_BLOCK))
            format = VK_FORMAT_BC7_UNORM_BLOCK;

        if (!unorm && (format == VK_FORMAT_BC7_UNORM_BLOCK))
            format = VK_FORMAT_BC7_SRGB_BLOCK;

        res.Width  = baseWidth;
        res.Height = baseHeight;
        res.Mips   = mips;
        res.Format = format;
        res.Data   = static_cast<void *>(image);
        res.Size   = dataSize;
        res.mType  = Type::Ktx;

        // Store texture handle to use when freeing memory:
        res.mExtra = static_cast<void *>(texture);
    }
    else
    {
        int32_t width, height, channels;

        //'STBI_rgb_alpha' forces 4 channels, even if source image has less:
        stbi_uc *pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

        vassert(pixels != nullptr,
                "Failed to load texture image. Filepath: " + std::string(path));

        res.Width  = width;
        res.Height = height;
        res.Mips   = MipStrategy::Generate;
        res.Format = unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
        res.Data   = static_cast<void *>(pixels);
        res.Size   = width * height * BytesPerPixel(res.Format);
        res.mType  = Type::Stb;
    }

    return res;
}

ImageData ImageData::ImportHDRI(const char *path)
{
    int32_t     width, height;
    float      *data;
    const char *err = nullptr;

    int32_t ret = LoadEXR(&data, &width, &height, path, &err);

    vassert(ret == TINYEXR_SUCCESS,
            "Error when trying to open image: " + std::string(path));

    auto res = ImageData();

    res.Name   = std::filesystem::path(path).stem().string();
    res.Width  = width;
    res.Height = height;
    res.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
    res.Data   = static_cast<void *>(data);
    res.Size   = width * height * BytesPerPixel(res.Format);
    res.mType  = Type::Exr;

    return res;
}

ImageData::ImageData(ImageData &&other) noexcept
    : Name(std::move(other.Name)), Width(other.Width), Height(other.Height),
      Mips(other.Mips), NumMips(other.NumMips), MipOffsets(std::move(other.MipOffsets)),
      Format(other.Format), Data(other.Data), Size(other.Size),
      IsUpToDate(other.IsUpToDate), mType(other.mType), mExtra(other.mExtra)
{
    other.Data   = nullptr;
    other.mType  = Type::None;
    other.mExtra = nullptr;
}

ImageData &ImageData::operator=(ImageData &&other) noexcept
{
    Name       = std::move(other.Name);
    Width      = other.Width;
    Height     = other.Height;
    Mips       = other.Mips;
    NumMips    = other.NumMips;
    MipOffsets = std::move(other.MipOffsets);
    Format     = other.Format;
    Data       = other.Data;
    Size       = other.Size;
    mType      = other.mType;
    mExtra     = other.mExtra;

    other.Data   = nullptr;
    other.mType  = Type::None;
    other.mExtra = nullptr;

    return *this;
}

ImageData::~ImageData()
{
    switch (mType)
    {
    case Type::None: {
        break;
    }
    case Type::Pixel: {
        auto ptr = static_cast<Pixel *>(Data);
        delete ptr;
        break;
    }
    case Type::Ktx: {
        auto tex = static_cast<ktxTexture *>(mExtra);
        ktxTexture_Destroy(tex);
        break;
    }
    case Type::Stb: {
        stbi_image_free(Data);
        break;
    }
    case Type::Exr: {
        free(Data);
        break;
    }
    }
}

bool ImageData::IsSinglePixel() const
{
    return mType == Type::Pixel;
}

glm::vec4 ImageData::GetPixelData() const
{
    vassert(mType == Type::Pixel);

    auto ptr = static_cast<Pixel *>(Data);

    return glm::vec4{
        static_cast<float>(ptr->R) / 255.0f,
        static_cast<float>(ptr->G) / 255.0f,
        static_cast<float>(ptr->B) / 255.0f,
        static_cast<float>(ptr->A) / 255.0f,
    };
}

void ImageData::UpdatePixelData(glm::vec4 v)
{
    vassert(mType == Type::Pixel);

    auto ptr = static_cast<Pixel *>(Data);

    *ptr = Pixel{
        .R = static_cast<uint8_t>(255.0f * v.x),
        .G = static_cast<uint8_t>(255.0f * v.y),
        .B = static_cast<uint8_t>(255.0f * v.z),
        .A = static_cast<uint8_t>(255.0f * v.w),
    };

    IsUpToDate = false;
}