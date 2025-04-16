#include "ImageData.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

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

    res.mType = Type::Pixel;

    return res;
}

ImageData ImageData::ImportSTB(const std::string &path, bool unorm)
{
    int width, height, channels;

    //'STBI_rgb_alpha' forces 4 channels, even if source image has less:
    stbi_uc *pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels)
    {
        std::string err_msg = "Failed to load texture image!\n";
        err_msg += "Filepath: " + path;

        throw std::runtime_error(err_msg);
    }

    auto res = ImageData();

    res.Width = width;
    res.Height = height;
    res.Channels = 4; // hence value of 4 here
    res.BytesPerChannel = 1;
    res.Unorm = unorm;

    res.Data = static_cast<void *>(pixels);

    res.mType = Type::Stb;

    return res;
}

ImageData ImageData::ImportEXR(const std::string &path)
{
    int width, height;
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

    auto res = ImageData();

    res.Width = width;
    res.Height = height;
    res.Channels = 4;
    res.BytesPerChannel = 4;

    res.Data = static_cast<void *>(data);

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