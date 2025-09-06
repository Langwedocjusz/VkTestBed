#include "ImageData.h"
#include "Pch.h"

#include "Vassert.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <filesystem>

ImageData ImageData::SinglePixel(Pixel p, bool unorm)
{
    auto data = new Pixel(p);

    auto res = ImageData();

    res.Width = 1;
    res.Height = 1;
    res.Format = unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
    res.Data = static_cast<void *>(data);
    res.Name = "SinglePixel";
    res.mType = Type::Pixel;

    return res;
}

ImageData ImageData::ImportSTB(const char *path, bool unorm)
{
    int width, height, channels;

    //'STBI_rgb_alpha' forces 4 channels, even if source image has less:
    stbi_uc *pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

    vassert(pixels != nullptr,
            "Failed to load texture image. Filepath: " + std::string(path));

    auto res = ImageData();

    res.Width = width;
    res.Height = height;
    res.Format = unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
    res.Data = static_cast<void *>(pixels);
    res.Name = std::filesystem::path(path).stem().string();
    res.mType = Type::Stb;

    return res;
}

ImageData ImageData::ImportEXR(const char *path)
{
    int width, height;
    float *data;
    const char *err = nullptr;

    int ret = LoadEXR(&data, &width, &height, path, &err);

    vassert(ret == TINYEXR_SUCCESS,
            "Error when trying to open image: " + std::string(path));

    auto res = ImageData();

    res.Width = width;
    res.Height = height;
    res.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
    res.Data = static_cast<void *>(data);
    res.Name = std::filesystem::path(path).stem().string();
    res.mType = Type::Exr;

    return res;
}

ImageData::ImageData(ImageData &&other) noexcept
    : Width(other.Width), Height(other.Height), Format(other.Format), Data(other.Data),
      mType(other.mType)
{
    other.Data = nullptr;
    other.mType = Type::None;
}

ImageData &ImageData::operator=(ImageData &&other) noexcept
{
    Width = other.Width;
    Height = other.Height;
    Format = other.Format;
    Data = other.Data;
    Name = std::move(other.Name);
    mType = other.mType;

    other.Data = nullptr;
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