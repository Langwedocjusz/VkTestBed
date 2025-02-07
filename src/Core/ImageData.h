#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct Pixel {
    uint8_t R;
    uint8_t G;
    uint8_t B;
    uint8_t A;
};

/// An owning handle to cpu side image data
class ImageData {
  public:
    static std::unique_ptr<ImageData> SinglePixel(Pixel p);
    static std::unique_ptr<ImageData> ImportSTB(const std::string &path);
    static std::unique_ptr<ImageData> ImportEXR(const std::string &path);

    ImageData() = default;
    ~ImageData();

  public:
    int Width = 0;
    int Height = 0;
    int Channels = 0;
    int BytesPerChannel = 0;

    void *Data = nullptr;

  private:
    enum class Type
    {
        None,
        Pixel,
        Stb,
        Exr,
    };

    Type mType = Type::None;
};