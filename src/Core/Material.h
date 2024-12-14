#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <variant>

#include <glm/glm.hpp>

enum class MaterialValueType : uint32_t
{
    Float,
    Vec2,
    Vec3,
    Vec4
};

struct MaterialKey {
    std::string Name;
    MaterialValueType Type;

    bool operator==(const MaterialKey &other) const
    {
        if (Name == other.Name && Type == other.Type)
            return true;
        else
            return false;
    }
};

namespace std
{
template <>
struct hash<MaterialKey> {
    size_t operator()(const MaterialKey &key) const
    {
        // Based on: https://en.cppreference.com/w/cpp/utility/hash
        std::size_t h1 = std::hash<std::string>{}(key.Name);
        std::size_t h2 = std::hash<MaterialValueType>{}(key.Type);

        return h1 ^ (h2 << 1);
    }
};
} // namespace std

class Material {
  public:
    static const MaterialKey Albedo;
    static const MaterialKey Normal;
    static const MaterialKey Roughness;
    static const MaterialKey Metallic;
    static const MaterialKey AlphaCutoff;

    enum class ImageChannel
    {
        None,
        R,
        G,
        B,
        A,
        RGB,
        GBA,
        RGBA
    };

    struct ImageSource {
        std::filesystem::path Path;
        ImageChannel Channel = ImageChannel::None;
    };

    using Value = std::variant<ImageSource, float, glm::vec2, glm::vec3, glm::vec4>;

  public:
    std::string Name;

    [[nodiscard]] size_t count(const MaterialKey &key) const
    {
        return mAttributes.count(key);
    }

    Value &operator[](const MaterialKey &key)
    {
        // To-do: enforce "type checking"
        // by comparing variant of value with
        // Key.Type
        return mAttributes[key];
    }

    auto begin()
    {
        return mAttributes.begin();
    }

    auto end()
    {
        return mAttributes.end();
    }

  private:
    std::unordered_map<MaterialKey, Value> mAttributes;
};