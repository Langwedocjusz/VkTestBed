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
// TODO: Refactor with template to avoid duplication (will probably need CRTP to keep type
// safety...)

namespace vkenum
{
namespace image_usage
{
struct ImageUsage {
    VkImageUsageFlags value;

    constexpr operator VkImageUsageFlags() const
    {
        return value;
    }

    constexpr ImageUsage operator|(ImageUsage other) const
    {
        return {value | other.value};
    }
};

constexpr ImageUsage TransferSrc{VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
constexpr ImageUsage TransferDst{VK_IMAGE_USAGE_TRANSFER_DST_BIT};
constexpr ImageUsage Storage{VK_IMAGE_USAGE_STORAGE_BIT};
constexpr ImageUsage Sampled{VK_IMAGE_USAGE_SAMPLED_BIT};
constexpr ImageUsage ColorAttachment{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
constexpr ImageUsage DepthStencilAttachment{VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};
}; // namespace image_usage

namespace buffer_usage
{
struct BufferUsage {
    VkBufferUsageFlags value;

    constexpr operator VkBufferUsageFlags() const
    {
        return value;
    }

    constexpr BufferUsage operator|(BufferUsage other) const
    {
        return {value | other.value};
    }
};

constexpr BufferUsage Storage{VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
constexpr BufferUsage TransferDst{VK_BUFFER_USAGE_TRANSFER_DST_BIT};
}; // namespace buffer_usage

}; // namespace vkenum
