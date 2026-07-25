#include "ImageData.h"
#include "Pch.h"

#include "Vassert.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYKTX_IMPLEMENTATION
#include "tinyktx.h"

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <filesystem>
#include <fstream>
#include <iostream>
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
        // Read entire file to memory:
        struct FileHandle {
            char   *Data;
            int64_t Size;
            size_t  CurrentByte = 0;
        } fileHandle;

        {
            // This automatically puts as at the end:
            std::ifstream file(path, std::ios::binary | std::ios::ate);

            if (!file)
            {
                auto msg = std::format("Failed to open file: {}", path);
                vpanic(msg);
            }

            // So we can recover file-size this way:
            fileHandle.Size = static_cast<int64_t>(file.tellg());

            // And read the whole thing:
            file.seekg(0, file.beg);

            fileHandle.Data = new char[fileHandle.Size];
            file.read(fileHandle.Data, fileHandle.Size);
        }

        // Initialize tiny_ktx context:
        auto tinyktxCallbackError = []([[maybe_unused]] void *user, char const *msg) {
            std::cerr << "Tiny_Ktx ERROR: " << msg << '\n';
        };

        auto tinyktxCallbackAlloc = []([[maybe_unused]] void *user,
                                       size_t                 size) -> void                 *{
            auto ptr = new uint8_t[size];
            return static_cast<void *>(ptr);
        };

        auto tinyktxCallbackFree = []([[maybe_unused]] void *user, void *data) {
            auto ptr = static_cast<uint8_t *>(data);
            delete[] ptr;
        };

        auto tinyktxCallbackRead = [](void *user, void *dest, size_t size) -> size_t {
            auto fileHandle = static_cast<FileHandle *>(user);

            size_t remaining = fileHandle->Size - fileHandle->CurrentByte;
            size_t toRead    = std::min(size, remaining);

            if (toRead > 0)
            {
                auto srcPtr = fileHandle->Data + fileHandle->CurrentByte;
                std::memcpy(dest, srcPtr, toRead);

                fileHandle->CurrentByte += toRead;
            }

            return toRead;
        };

        auto tinyktxCallbackSeek = [](void *user, int64_t offset) -> bool {
            auto fileHandle = static_cast<FileHandle *>(user);

            // Assume file is smaller than int64 max.
            if (offset < 0 || offset >= static_cast<int64_t>(fileHandle->Size))
            {
                return false;
            }

            fileHandle->CurrentByte = offset;

            return true;
        };

        auto tinyktxCallbackTell = [](void *user) -> int64_t {
            auto fileHandle = static_cast<FileHandle *>(user);

            return static_cast<int64_t>(fileHandle->CurrentByte);
        };

        TinyKtx_Callbacks callbacks{.errorFn = tinyktxCallbackError,
                                    .allocFn = tinyktxCallbackAlloc,
                                    .freeFn  = tinyktxCallbackFree,
                                    .readFn  = tinyktxCallbackRead,
                                    .seekFn  = tinyktxCallbackSeek,
                                    .tellFn  = tinyktxCallbackTell};

        auto ctx = TinyKtx_CreateContext(&callbacks, &fileHandle);

        TinyKtx_ReadHeader(ctx);
        uint32_t       baseWidth  = TinyKtx_Width(ctx);
        uint32_t       baseHeight = TinyKtx_Height(ctx);
        uint32_t       baseDepth  = TinyKtx_Depth(ctx);
        uint32_t       slices     = TinyKtx_ArraySlices(ctx);
        TinyKtx_Format fmt        = TinyKtx_GetFormat(ctx);

        vassert(fmt != TKTX_UNDEFINED, "Image format not defined!");

        if (baseDepth > 1)
        {
            auto msg = std::format("3D Images currently not supported! Got {} depth.",
                                   baseDepth);
            vpanic(msg);
        }

        if (slices > 1)
        {
            auto msg = std::format(
                "Texture Arrays currently not supported! Got {} slices.", slices);
            vpanic(msg);
        }

        // Ktx formats are equal to VkFormats where possible:
        auto format = static_cast<VkFormat>(fmt);

        // TODO: this is a horrible hack:
        if (unorm && (format == VK_FORMAT_BC7_SRGB_BLOCK))
            format = VK_FORMAT_BC7_UNORM_BLOCK;

        if (!unorm && (format == VK_FORMAT_BC7_UNORM_BLOCK))
            format = VK_FORMAT_BC7_SRGB_BLOCK;

        // Precalculate image size and mip offsets:
        size_t imageBytes = 0;

        for (uint32_t mip = 0; mip < TinyKtx_NumberOfMipmaps(ctx); mip++)
        {
            res.MipOffsets.push_back(imageBytes);
            imageBytes += TinyKtx_ImageSize(ctx, mip);
        }

        // Allocate memory and copy all image levels:
        auto ourData = new uint8_t[imageBytes];

        size_t currentOffset = 0;

        for (uint32_t mip = 0; mip < TinyKtx_NumberOfMipmaps(ctx); mip++)
        {
            auto currentSize = TinyKtx_ImageSize(ctx, mip);

            std::memcpy(ourData + currentOffset, TinyKtx_ImageRawData(ctx, mip),
                        currentSize);

            currentOffset += currentSize;
        }

        res.Width  = baseWidth;
        res.Height = baseHeight;
        // TODO: Should branch on wether or not mips are present.
        // But that would require compute shaders that can write
        // to compressed images...
        res.Mips    = MipStrategy::Load;
        res.NumMips = static_cast<uint32_t>(res.MipOffsets.size());
        res.Format  = format;
        res.Data    = static_cast<void *>(ourData);
        res.Size    = imageBytes;
        res.mType   = Type::Ktx;

        // This apparently also frees any image memory
        // tiny_ktx allocated along the way:
        TinyKtx_DestroyContext(ctx);
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
        auto ptr = static_cast<uint8_t *>(Data);
        delete[] ptr;
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