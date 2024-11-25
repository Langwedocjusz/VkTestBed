#pragma once

#include <string>
#include <variant>
#include <filesystem>
#include <unordered_map>

#include <glm/glm.hpp>

enum class MaterialValueType : uint32_t {
    Float, Vec2, Vec3, Vec4
};

struct MaterialKey{
    std::string Name;
    MaterialValueType Type;

    bool operator == (const MaterialKey &other) const
    {
        if (Name == other.Name && Type == other.Type)
            return true;
        else
            return false;
    }
};

namespace std {
  template <> struct hash<MaterialKey>
  {
    size_t operator()(const MaterialKey& key) const
    {
        //Based on: https://en.cppreference.com/w/cpp/utility/hash
        std::size_t h1 = std::hash<std::string>{}(key.Name);
        std::size_t h2 = std::hash<MaterialValueType>{}(key.Type);

        return h1 ^ (h2 << 1);
    }
  };
}

class Material{
public:
    static constexpr MaterialKey Albedo = MaterialKey{"Albedo", MaterialValueType::Vec3};
    static constexpr MaterialKey Normal = MaterialKey{"Normal", MaterialValueType::Vec3};
    static constexpr MaterialKey Roughness = MaterialKey{"Roughness", MaterialValueType::Float};
    static constexpr MaterialKey Metallic = MaterialKey{"Metallic", MaterialValueType::Float};

    enum class ImageChannel{
        R, G, B, A,
        RGB, GBA
    };

    struct ImageSource{
        std::filesystem::path Path;
        ImageChannel Channel;
    };

    using Value = std::variant<ImageSource, float, glm::vec2, glm::vec3, glm::vec4>;
public:
    std::string Name;

    [[nodiscard]] size_t count(const MaterialKey& key) const
    {
        return mAttributes.count(key);
    }

    Value& operator[](const MaterialKey& key)
    {
        //To-do: enforce "type checking"
        //by comparing variant of value with
        //Key.Type
        return mAttributes[key];
    }
private:
    std::unordered_map<MaterialKey, Value> mAttributes;
};