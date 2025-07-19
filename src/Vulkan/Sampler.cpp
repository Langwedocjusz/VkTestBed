#include "Sampler.h"
#include "Pch.h"

#include "VkUtils.h"

#include <vulkan/vulkan.h>

#include "Vassert.h"

SamplerBuilder::SamplerBuilder(std::string_view debugName) : mDebugName(debugName)
{
}

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
    return BuildImpl(ctx);
}

VkSampler SamplerBuilder::Build(VulkanContext &ctx, DeletionQueue &queue)
{
    const auto res = BuildImpl(ctx);

    queue.push_back(res);

    return res;
}

VkSampler SamplerBuilder::BuildImpl(VulkanContext &ctx)
{
    VkSampler sampler{};

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

    auto ret = vkCreateSampler(ctx.Device, &samplerInfo, nullptr, &sampler);

    vassert(ret == VK_SUCCESS, "Failed to create texture sampler!");

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_SAMPLER, sampler, mDebugName);

    return sampler;
}