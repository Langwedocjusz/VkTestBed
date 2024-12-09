#include "Descriptor.h"
#include <cstdint>

DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::AddBinding(uint32_t binding,
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
                                      std::span<VkDescriptorPoolSize> poolSizes)
{
    VkDescriptorPool pool;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(ctx.Device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool!");

    return pool;
}

VkDescriptorSet Descriptor::Allocate(VulkanContext &ctx, VkDescriptorPool pool,
                                     VkDescriptorSetLayout &layout)
{
    VkDescriptorSet descriptorSet{};

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(ctx.Device, &allocInfo, &descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor sets!");

    return descriptorSet;
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

DescriptorUpdater::DescriptorUpdater(VkDescriptorSet descriptorSet)
    : mDescriptorSet(descriptorSet)
{
}

DescriptorUpdater &DescriptorUpdater::WriteUniformBuffer(uint32_t binding,
                                                         VkBuffer buffer,
                                                         VkDeviceSize size)
{
    auto &bufferInfo = mBufferInfos.emplace_back();
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = size;

    auto &descriptorWrite = mWrites.emplace_back();
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = mDescriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    return *this;
}

DescriptorUpdater &DescriptorUpdater::WriteImage(uint32_t binding, VkImageView imageView,
                                                 VkSampler sampler)
{
    auto &imageInfo = mImageInfos.emplace_back();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    auto &descriptorWrite = mWrites.emplace_back();
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = mDescriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    return *this;
}

void DescriptorUpdater::Update(VulkanContext &ctx)
{
    vkUpdateDescriptorSets(ctx.Device, mWrites.size(), mWrites.data(), 0, nullptr);
}

DescriptorAllocator::DescriptorAllocator(VulkanContext &ctx) : mCtx(ctx)
{
}

void DescriptorAllocator::OnInit(std::span<VkDescriptorPoolSize> sizes)
{
    mCountsPerSet.assign(sizes.begin(), sizes.end());

    mReadyPools.push_back(CreatePool());
    GrowSetsPerPool();
}

VkDescriptorSet DescriptorAllocator::Allocate(VkDescriptorSetLayout &layout)
{
    VkDescriptorPool pool = GetPool();

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    VkResult res = vkAllocateDescriptorSets(mCtx.Device, &allocInfo, &set);

    if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL)
    {
        mFullPools.push_back(pool);

        pool = GetPool();
        allocInfo.descriptorPool = pool;

        if (vkAllocateDescriptorSets(mCtx.Device, &allocInfo, &set) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    mReadyPools.push_back(pool);

    return set;
}

std::vector<VkDescriptorSet> DescriptorAllocator::Allocate(
    std::span<VkDescriptorSetLayout> layouts)
{
    VkDescriptorPool pool = GetPool();

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = layouts.size();
    allocInfo.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> sets(layouts.size());
    VkResult res = vkAllocateDescriptorSets(mCtx.Device, &allocInfo, sets.data());

    if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL)
    {
        mFullPools.push_back(pool);

        pool = GetPool();
        allocInfo.descriptorPool = pool;

        if (vkAllocateDescriptorSets(mCtx.Device, &allocInfo, sets.data()) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    mReadyPools.push_back(pool);

    return sets;
}

VkDescriptorPool DescriptorAllocator::GetPool()
{
    VkDescriptorPool pool;

    if (mReadyPools.size() != 0)
    {
        pool = mReadyPools.back();
        mReadyPools.pop_back();
    }

    else
    {
        pool = CreatePool();
        GrowSetsPerPool();
    }

    return pool;
}

VkDescriptorPool DescriptorAllocator::CreatePool()
{
    std::vector<VkDescriptorPoolSize> poolSizes = mCountsPerSet;

    for (auto &elem : poolSizes)
        elem.descriptorCount *= mSetsPerPool;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = 0;
    poolInfo.maxSets = mSetsPerPool;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(mCtx.Device, &poolInfo, nullptr, &newPool);

    return newPool;
}

void DescriptorAllocator::GrowSetsPerPool()
{
    constexpr double growthFactor = 1.5f;
    constexpr uint32_t maxSetsPerPool = 4096;

    mSetsPerPool = static_cast<uint32_t>(growthFactor * mSetsPerPool);
    mSetsPerPool = std::min(mSetsPerPool, maxSetsPerPool);
}

void DescriptorAllocator::ResetPools()
{
    for (auto pool : mReadyPools)
        vkResetDescriptorPool(mCtx.Device, pool, 0);

    for (auto pool : mFullPools)
    {
        vkResetDescriptorPool(mCtx.Device, pool, 0);
        mReadyPools.push_back(pool);
    }

    mFullPools.clear();
}

void DescriptorAllocator::DestoyPools()
{
    for (auto pool : mReadyPools)
        vkDestroyDescriptorPool(mCtx.Device, pool, nullptr);

    for (auto pool : mFullPools)
        vkDestroyDescriptorPool(mCtx.Device, pool, nullptr);

    mReadyPools.clear();
    mFullPools.clear();
}