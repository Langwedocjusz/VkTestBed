#include "Sampler.h"

SamplerBuilder &SamplerBuilder::SetMagFilter(VkFilter filter)
{
    mMagFiler = filter;
    return *this;
}

SamplerBuilder &SamplerBuilder::SetMinFilter(VkFilter filter)
{
    mMinFiler = filter;
    return *this;
}

SamplerBuilder &SamplerBuilder::SetAddressMode(VkSamplerAddressMode adressMode)
{
    mAddressMode = adressMode;
    return *this;
}

SamplerBuilder &SamplerBuilder::SetMipmapMode(VkSamplerMipmapMode mipmapMode)
{
    mMipmapMode = mipmapMode;
    return *this;
}

SamplerBuilder &SamplerBuilder::SetMaxLod(float maxLod)
{
    mMaxLod = maxLod;
    return *this;
}

VkSampler SamplerBuilder::Build(VulkanContext &ctx)
{
    VkSampler sampler;

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = mMagFiler;
    samplerInfo.minFilter = mMinFiler;
    samplerInfo.addressModeU = mAddressMode;
    samplerInfo.addressModeV = mAddressMode;
    samplerInfo.addressModeW = mAddressMode;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(ctx.PhysicalDevice, &properties);

    // To-do: Un-hardcode those samper parameters:
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.mipmapMode = mMipmapMode;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = mMaxLod;

    if (vkCreateSampler(ctx.Device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture sampler!");

    return sampler;
}