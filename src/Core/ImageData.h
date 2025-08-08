#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vulkan/vulkan_core.h>

struct Pixel {
    uint8_t R;
    uint8_t G;
    uint8_t B;
    uint8_t A;
};

/// An owning handle to cpu side image data
class ImageData {
  public:
    static ImageData SinglePixel(Pixel p, bool unorm);
    static ImageData ImportSTB(const std::filesystem::path &path, bool unorm);
    static ImageData ImportEXR(const std::filesystem::path &path);

    ImageData() = default;
    ~ImageData();

    ImageData(const ImageData &) = delete;
    ImageData &operator=(const ImageData &) = delete;

    ImageData(ImageData &&) noexcept;
    ImageData &operator=(ImageData &&) noexcept;

  public:
    std::string Name;

    int Width = 0;
    int Height = 0;
    VkFormat Format;

    void *Data = nullptr;

  private:
    // Strategy of freeing memory depends on what library was used
    // to import the image, hence this enum.
    enum class Type
    {
        None,
        Pixel,
        Stb,
        Exr,
    };

    Type mType = Type::None;
};