#pragma once

#include "VulkanContext.h"

class SamplerBuilder {
  public:
    SamplerBuilder() = default;

    SamplerBuilder SetMagFilter(VkFilter filter);
    SamplerBuilder SetMinFilter(VkFilter filter);
    SamplerBuilder SetAddressMode(VkSamplerAddressMode adressMode);

    VkSampler Build(VulkanContext &ctx);

  private:
    VkFilter mMagFiler;
    VkFilter mMinFiler;
    VkSamplerAddressMode mAddressMode;
};