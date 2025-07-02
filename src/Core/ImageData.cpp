#include "ImageData.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <libassert/assert.hpp>

#include <format>

ImageData ImageData::SinglePixel(Pixel p, bool unorm)
{
    auto data = new Pixel(p);

    auto res = ImageData();

    res.Width = 1;
    res.Height = 1;
    res.Channels = 4;
    res.BytesPerChannel = 1;
    res.Unorm = unorm;
    res.Data = static_cast<void *>(data);
    res.Name = "SinglePixel";
    res.mType = Type::Pixel;

    return res;
}

ImageData ImageData::ImportSTB(const std::filesystem::path &path, bool unorm)
{
    int width, height, channels;

    //'STBI_rgb_alpha' forces 4 channels, even if source image has less:
    stbi_uc *pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    ASSERT(pixels != nullptr,
           std::format("Failed to load texture image. Filepath: {}", path.string()));

    auto res = ImageData();

    res.Width = width;
    res.Height = height;
    res.Channels = 4; // hence value of 4 here
    res.BytesPerChannel = 1;
    res.Unorm = unorm;
    res.Data = static_cast<void *>(pixels);
    res.Name = path.stem();
    res.mType = Type::Stb;

    return res;
}

ImageData ImageData::ImportEXR(const std::filesystem::path &path)
{
    int width, height;
    float *data;
    const char *err = nullptr;

    int ret = LoadEXR(&data, &width, &height, path.c_str(), &err);

    ASSERT(ret == TINYEXR_SUCCESS,
           std::format("Error when trying to open image: {}", path.string()));

    auto res = ImageData();

    res.Width = width;
    res.Height = height;
    res.Channels = 4;
    res.BytesPerChannel = 4;
    res.Data = static_cast<void *>(data);
    res.Name = path.stem();
    res.mType = Type::Exr;

    return res;
}

ImageData::ImageData(ImageData &&other) noexcept
    : Width(other.Width), Height(other.Height), Channels(other.Channels),
      BytesPerChannel(other.BytesPerChannel), Unorm(other.Unorm), Data(other.Data),
      mType(other.mType)
{
    other.Data = nullptr;
    other.mType = Type::None;
}

ImageData &ImageData::operator=(ImageData &&other) noexcept
{
    Width = other.Width;
    Height = other.Height;
    Channels = other.Channels;
    BytesPerChannel = other.BytesPerChannel;
    Unorm = other.Unorm;
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