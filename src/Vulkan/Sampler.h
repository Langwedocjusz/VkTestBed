#pragma once

#include "VulkanContext.h"
#include <vulkan/vulkan_core.h>

class SamplerBuilder {
  public:
    SamplerBuilder() = default;

    SamplerBuilder &SetMagFilter(VkFilter filter);
    SamplerBuilder &SetMinFilter(VkFilter filter);
    SamplerBuilder &SetAddressMode(VkSamplerAddressMode adressMode);
    SamplerBuilder &SetMipmapMode(VkSamplerMipmapMode mipmapMode);
    SamplerBuilder &SetMaxLod(float maxLod);

    VkSampler Build(VulkanContext &ctx);

  private:
    VkFilter mMagFiler;
    VkFilter mMinFiler;
    VkSamplerAddressMode mAddressMode;
    VkSamplerMipmapMode mMipmapMode;
    float mMaxLod;
};