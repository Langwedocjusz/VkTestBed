#include "ImageData.h"
#include "Pch.h"

#include "FileHandle.h"
#include "Path.h"
#include "Vassert.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYKTX_IMPLEMENTATION
#include "tinyktx.h"

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#include <cstddef>
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

ImageData ImageData::ImportImage(const std::string &path, bool unorm)
{
    Path pathObj(path);

    ImageData res{};
    res.Name = pathObj.Stem();

    // Open file handle:
    FileHandle file(path);

    if (pathObj.Extension() == ".ktx" || pathObj.Extension() == ".ktx2")
    {
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
            auto file = static_cast<FileHandle *>(user);

            file->Read(static_cast<char *>(dest), static_cast<int64_t>(size));
            return static_cast<size_t>(file->GCount());
        };

        auto tinyktxCallbackSeek = [](void *user, int64_t offset) -> bool {
            auto file = static_cast<FileHandle *>(user);

            if (offset < 0 || offset > file->Size())
            {
                return false;
            }

            // Clearing previous error flags:
            file->Clear();
            file->SeekG(offset, FileHandle::Dir::Beg);

            return file->Good();
        };

        auto tinyktxCallbackTell = [](void *user) -> int64_t {
            auto fileHandle = static_cast<FileHandle *>(user);

            // Clear previous error flags:
            fileHandle->Clear();
            return fileHandle->TellG();
        };

        TinyKtx_Callbacks callbacks{.errorFn = tinyktxCallbackError,
                                    .allocFn = tinyktxCallbackAlloc,
                                    .freeFn  = tinyktxCallbackFree,
                                    .readFn  = tinyktxCallbackRead,
                                    .seekFn  = tinyktxCallbackSeek,
                                    .tellFn  = tinyktxCallbackTell};

        auto ctx = TinyKtx_CreateContext(&callbacks, &file);

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
        stbi_io_callbacks callbacks{
            .read = [](void *user, char *data, int size) -> int {
                auto *file = static_cast<FileHandle *>(user);

                file->Clear();
                file->Read(data, size);

                return static_cast<int>(file->GCount());
            },

            .skip = [](void *user, int n) -> void {
                auto *file = static_cast<FileHandle *>(user);

                file->Clear();
                file->SeekG(n, FileHandle::Dir::Cur);
            },

            .eof = [](void *user) -> int {
                auto *file = static_cast<FileHandle *>(user);

                auto pos = file->TellG();
                return (pos < 0 || pos >= file->Size());
            },
        };

        int32_t width, height, channels;

        //'STBI_rgb_alpha' forces 4 channels, even if source image has less:
        stbi_uc *pixels = stbi_load_from_callbacks(&callbacks, &file, &width, &height,
                                                   &channels, STBI_rgb_alpha);

        vassert(pixels != nullptr,
                std::format("Failed to load texture image. Filepath: {}",
                            reinterpret_cast<const char *>(path.c_str())));

        res.Width  = width;
        res.Height = height;
        res.Mips   = MipStrategy::Generate;
        res.Format = unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
        res.Data   = static_cast<void *>(pixels);
        res.Size  = static_cast<VkDeviceSize>(width * height) * BytesPerPixel(res.Format);
        res.mType = Type::Stb;
    }

    return res;
}

ImageData ImageData::ImportHDRI(const std::string &path)
{
    // Preload image to ram. Fstream returns char*
    // while tinyexr expects unsinged char*,
    // so we reinterpret cast here:
    std::vector<unsigned char> fileData{};

    FileHandle file(path);
    fileData.resize(file.Size());
    file.Read(reinterpret_cast<char *>(fileData.data()), file.Size());

    int32_t     width, height;
    float      *data;
    const char *err = nullptr;

    auto ret =
        LoadEXRFromMemory(&data, &width, &height, fileData.data(), fileData.size(), &err);

    vassert(ret == TINYEXR_SUCCESS,
            std::format("Error when trying to open image: {}",
                        reinterpret_cast<const char *>(path.c_str())));

    auto res = ImageData();

    res.Name   = Path(path).Stem();
    res.Width  = width;
    res.Height = height;
    res.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
    res.Data   = static_cast<void *>(data);
    res.Size   = static_cast<VkDeviceSize>(width * height) * BytesPerPixel(res.Format);
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