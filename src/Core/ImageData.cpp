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

ImageData ImageData::SinglePixel(Pixel p, bool unorm)
{
    auto data = new Pixel(p);

    auto res = ImageData();

    res.Width  = 1;
    res.Height = 1;
    res.Mips   = MipStrategy::DoNothing;
    res.Format = unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
    res.Data   = static_cast<void *>(data);
    res.Name   = "SinglePixel";
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

        // TODO: Support more image types
        // ktx_uint32_t baseDepth = texture->baseDepth;
        // ktx_uint32_t numLevels = texture->numLevels;
        // ktx_bool_t isArray = texture->isArray;

        auto mips = texture->generateMipmaps ? MipStrategy::Generate : MipStrategy::Load;

        ktx_uint8_t *image = ktxTexture_GetData(texture);

        res.Width  = baseWidth;
        res.Height = baseHeight;
        res.Mips   = mips;
        res.Format = ktxTexture_GetVkFormat(texture);
        res.Data   = static_cast<void *>(image);
        res.mType  = Type::Ktx;

        // Store texture handle to use when freeing memory:
        res.mExtra = static_cast<void *>(texture);

        // ...
        // Do something with the texture image.
        // ...
    }
    else
    {
        int width, height, channels;

        //'STBI_rgb_alpha' forces 4 channels, even if source image has less:
        stbi_uc *pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

        vassert(pixels != nullptr,
                "Failed to load texture image. Filepath: " + std::string(path));

        res.Width  = width;
        res.Height = height;
        res.Mips   = MipStrategy::Generate;
        res.Format = unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
        res.Data   = static_cast<void *>(pixels);
        res.mType  = Type::Stb;
    }

    return res;
}

ImageData ImageData::ImportHDRI(const char *path)
{
    int         width, height;
    float      *data;
    const char *err = nullptr;

    int ret = LoadEXR(&data, &width, &height, path, &err);

    vassert(ret == TINYEXR_SUCCESS,
            "Error when trying to open image: " + std::string(path));

    auto res = ImageData();

    res.Width  = width;
    res.Height = height;
    res.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
    res.Data   = static_cast<void *>(data);
    res.Name   = std::filesystem::path(path).stem().string();
    res.mType  = Type::Exr;

    return res;
}

ImageData::ImageData(ImageData &&other) noexcept
    : Width(other.Width), Height(other.Height), Format(other.Format), Data(other.Data),
      mType(other.mType)
{
    other.Data  = nullptr;
    other.mType = Type::None;
}

ImageData &ImageData::operator=(ImageData &&other) noexcept
{
    Width  = other.Width;
    Height = other.Height;
    Format = other.Format;
    Data   = other.Data;
    Name   = std::move(other.Name);
    mType  = other.mType;

    other.Data  = nullptr;
    other.mType = Type::None;

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