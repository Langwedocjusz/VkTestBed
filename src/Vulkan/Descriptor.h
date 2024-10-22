#pragma once

#include "VulkanContext.h"

#include <span>

class DescriptorSetLayoutBuilder {
  public:
    DescriptorSetLayoutBuilder() = default;

    DescriptorSetLayoutBuilder AddBinding(uint32_t binding, VkDescriptorType type,
                                          uint32_t stages);

    VkDescriptorSetLayout Build(VulkanContext &ctx);

  private:
    std::vector<VkDescriptorSetLayoutBinding> mBindings;
};

namespace Descriptor
{
struct PoolCount {
    VkDescriptorType Type;
    uint32_t Count;
};

VkDescriptorPool InitPool(VulkanContext &ctx, uint32_t maxSets,
                          std::span<PoolCount> poolCounts);

std::vector<VkDescriptorSet> Allocate(VulkanContext &ctx, VkDescriptorPool pool,
                                      std::span<VkDescriptorSetLayout> layouts);
}; // namespace Descriptor