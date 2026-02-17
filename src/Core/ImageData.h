#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

struct Pixel {
    uint8_t R;
    uint8_t G;
    uint8_t B;
    uint8_t A;
};

enum class MipStrategy
{
    DoNothing,
    Generate,
    Load,
};

/// An owning handle to cpu side image data
class ImageData {
  public:
    static ImageData SinglePixel(Pixel p, bool unorm);
    // Uses c_str as that is what is passed to the underlying libraries anyway
    // and using filesystem::path measurably bloats the compile time.
    static ImageData ImportImage(const char *path, bool unorm);
    static ImageData ImportHDRI(const char *path);

    ImageData() = default;
    ~ImageData();

    ImageData(const ImageData &)            = delete;
    ImageData &operator=(const ImageData &) = delete;

    ImageData(ImageData &&) noexcept;
    ImageData &operator=(ImageData &&) noexcept;

    [[nodiscard]] bool IsSinglePixel() const;

    [[nodiscard]] glm::vec4 GetPixelData() const;
    void                    UpdatePixelData(glm::vec4 v);

  public:
    std::string Name;

    // TODO: At the moment we only support single layer 2D images
    uint32_t    Width  = 0;
    uint32_t    Height = 0;
    MipStrategy Mips   = MipStrategy::DoNothing;
    VkFormat    Format;

    void *Data = nullptr;

    mutable bool IsUpToDate = false;

  private:
    // Strategy of freeing memory depends on what library was used
    // to import the image, hence this enum.
    enum class Type
    {
        None,
        Pixel,
        Stb,
        Exr,
        Ktx,
    };

    Type mType = Type::None;

    // Extra storage for backend-dependent data:
    void *mExtra;
};