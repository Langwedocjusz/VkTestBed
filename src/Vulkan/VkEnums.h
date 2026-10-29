#pragma once

// This exists solely to make various flag/format declarations
// when initializing vulkan structs less unreadable.
// For example:
// usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
//         VK_IMAGE_USAGE_TRANSFER_DST_BIT |
//         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
// becomes:
// usage = TransferSrc | TransferDst | Storage | Sampled;

// TODO: Add constants as needed...

#include "vk_mem_alloc.h"

#include <type_traits>

namespace vkenum
{
// Template helper to provide less type-unsafe wrapper for flag bits:
template <typename Bits>
class Flags {
  public:
    using Underlying = std::underlying_type_t<Bits>;

    constexpr Flags(Bits b)
        : mValue(static_cast<Underlying>(b))
    {
    }

    constexpr operator Underlying() const
    {
        return mValue;
    }

    constexpr Flags operator|(Flags other) const
    {
        return Flags{mValue | other.mValue};
    }

  private:
    constexpr explicit Flags(Underlying v)
        : mValue(v)
    {
    }
    Underlying mValue;
};

// Needed to prevent union between different Flag types:
template <typename A, typename B>
    requires(!std::is_same_v<A, B>)
void operator|(Flags<A>, Flags<B>) = delete;

namespace image_usage
{
using ImageUsage = Flags<VkImageUsageFlagBits>;

constexpr ImageUsage TransferSrc     = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
constexpr ImageUsage TransferDst     = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
constexpr ImageUsage Storage         = VK_IMAGE_USAGE_STORAGE_BIT;
constexpr ImageUsage Sampled         = VK_IMAGE_USAGE_SAMPLED_BIT;
constexpr ImageUsage ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
constexpr ImageUsage DepthStencil    = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
}; // namespace image_usage

namespace buffer_usage
{
using BufferUsage = Flags<VkBufferUsageFlagBits>;

constexpr BufferUsage UniformBuffer       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
constexpr BufferUsage StorageBuffer       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
constexpr BufferUsage VertexBuffer        = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
constexpr BufferUsage IndexBuffer         = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
constexpr BufferUsage TransferSrc         = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
constexpr BufferUsage TransferDst         = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
constexpr BufferUsage ShaderDeviceAddress = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
}; // namespace buffer_usage

namespace alloc_create
{
using AllocCreateFlags = Flags<VmaAllocationCreateFlagBits>;

constexpr AllocCreateFlags Mapped                    = VMA_ALLOCATION_CREATE_MAPPED_BIT;
constexpr AllocCreateFlags HostAccessSequentialWrite = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
constexpr AllocCreateFlags HostAccessRandom          = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
}; // namespace alloc_create

// Those are just plain enums, not bit flags, so type concerns from above don't apply:

namespace image_layout
{
constexpr VkImageLayout ColorAttachment = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
constexpr VkImageLayout DepthStencil    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
constexpr VkImageLayout General         = VK_IMAGE_LAYOUT_GENERAL;
constexpr VkImageLayout ShaderRead      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
constexpr VkImageLayout TransferDst     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
constexpr VkImageLayout TransferSrc     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
constexpr VkImageLayout Undefined       = VK_IMAGE_LAYOUT_UNDEFINED;
}; // namespace image_layout

}; // namespace vkenum
