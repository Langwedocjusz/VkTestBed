#pragma once

#include "DeletionQueue.h"
#include "VulkanContext.h"

#include <vulkan/vulkan.h>

#include <optional>
#include <string>
#include <string_view>

class SamplerBuilder {
  public:
    SamplerBuilder(std::string_view debugName);

    SamplerBuilder &SetMagFilter(VkFilter filter);
    SamplerBuilder &SetMinFilter(VkFilter filter);
    SamplerBuilder &SetAddressMode(VkSamplerAddressMode adressMode);
    SamplerBuilder &SetMipmapMode(VkSamplerMipmapMode mipmapMode);
    SamplerBuilder &SetMaxLod(float maxLod);
    SamplerBuilder &SetBorderColor(VkBorderColor color);
    SamplerBuilder &SetCompareOp(VkCompareOp op);

    VkSampler Build(VulkanContext &ctx);
    VkSampler Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    VkSampler BuildImpl(VulkanContext &ctx);

  private:
    std::optional<VkCompareOp> mCompareOp = std::nullopt;

    VkFilter mMagFiler = VK_FILTER_LINEAR;
    VkFilter mMinFiler = VK_FILTER_LINEAR;
    VkSamplerAddressMode mAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerMipmapMode mMipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VkBorderColor mBorderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    float mMaxLod = 0.0f;

    std::string mDebugName;
};