#include "Descriptor.h"

DescriptorSetLayoutBuilder DescriptorSetLayoutBuilder::AddBinding(uint32_t binding,
                                                                  VkDescriptorType type,
                                                                  uint32_t stages)
{
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.stageFlags = stages;
    // Hardcoded for now:
    layoutBinding.descriptorCount = 1;
    layoutBinding.pImmutableSamplers = nullptr;

    mBindings.push_back(layoutBinding);

    return *this;
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::Build(VulkanContext &ctx)
{
    VkDescriptorSetLayout layout;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(mBindings.size());
    layoutInfo.pBindings = mBindings.data();

    if (vkCreateDescriptorSetLayout(ctx.Device, &layoutInfo, nullptr, &layout) !=
        VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout!");

    return layout;
}

VkDescriptorPool Descriptor::InitPool(VulkanContext &ctx, uint32_t maxSets,
                                      std::span<PoolCount> poolCounts)
{
    VkDescriptorPool pool;

    std::vector<VkDescriptorPoolSize> poolSizes;

    for (PoolCount pc : poolCounts)
    {
        VkDescriptorPoolSize poolSize;
        poolSize.type = pc.Type;
        poolSize.descriptorCount = pc.Count;

        poolSizes.push_back(poolSize);
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(ctx.Device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool!");

    return pool;
}

std::vector<VkDescriptorSet> Descriptor::Allocate(
    VulkanContext &ctx, VkDescriptorPool pool, std::span<VkDescriptorSetLayout> layouts)
{
    std::vector<VkDescriptorSet> descriptorSets(layouts.size());

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(ctx.Device, &allocInfo, descriptorSets.data()) !=
        VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor sets!");

    return descriptorSets;
}