#include "Descriptor.h"
#include "Pch.h"

#include "Vassert.h"
#include "VkUtils.h"

#include "volk.h"

#include <cstdint>

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(std::string_view debugName)
    : mDebugName(debugName)
{
}

DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::AddBinding(uint32_t binding,
                                                                   VkDescriptorType type,
                                                                   uint32_t stages)
{
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding        = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.stageFlags     = stages;
    // Hardcoded for now:
    layoutBinding.descriptorCount    = 1;
    layoutBinding.pImmutableSamplers = nullptr;

    mBindings.push_back(layoutBinding);

    return *this;
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::Build(VulkanContext &ctx)
{
    return BuildImpl(ctx);
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::Build(VulkanContext &ctx,
                                                        DeletionQueue &queue)
{
    const auto res = BuildImpl(ctx);

    queue.push_back(res);

    return res;
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::BuildImpl(VulkanContext &ctx)
{
    VkDescriptorSetLayout layout{};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(mBindings.size());
    layoutInfo.pBindings    = mBindings.data();

    auto ret = vkCreateDescriptorSetLayout(ctx.Device, &layoutInfo, nullptr, &layout);

    vassert(ret == VK_SUCCESS, "Failed to create descriptor set layout!");

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, layout, mDebugName);

    return layout;
}

VkDescriptorPool Descriptor::InitPool(VulkanContext &ctx, uint32_t maxSets,
                                      std::span<VkDescriptorPoolSize> poolSizes)
{
    VkDescriptorPool pool;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();

    auto ret = vkCreateDescriptorPool(ctx.Device, &poolInfo, nullptr, &pool);

    vassert(ret == VK_SUCCESS, "Failed to create descriptor pool!");

    return pool;
}

VkDescriptorSet Descriptor::Allocate(VulkanContext &ctx, VkDescriptorPool pool,
                                     VkDescriptorSetLayout &layout)
{
    VkDescriptorSet descriptorSet{};

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &layout;

    auto ret = vkAllocateDescriptorSets(ctx.Device, &allocInfo, &descriptorSet);

    vassert(ret == VK_SUCCESS, "Failed to allocate descriptor sets!");

    return descriptorSet;
}

std::vector<VkDescriptorSet> Descriptor::Allocate(
    VulkanContext &ctx, VkDescriptorPool pool, std::span<VkDescriptorSetLayout> layouts)
{
    std::vector<VkDescriptorSet> descriptorSets(layouts.size());

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts        = layouts.data();

    auto ret = vkAllocateDescriptorSets(ctx.Device, &allocInfo, descriptorSets.data());

    vassert(ret == VK_SUCCESS, "Failed to allocate descriptor sets!");

    return descriptorSets;
}

DescriptorUpdater::DescriptorUpdater(VkDescriptorSet descriptorSet)
    : mDescriptorSet(descriptorSet)
{
}

DescriptorUpdater &DescriptorUpdater::WriteUniformBuffer(uint32_t     binding,
                                                         VkBuffer     buffer,
                                                         VkDeviceSize size)
{
    auto &bufferInfo  = mBufferInfos.emplace_back();
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = size;

    mWriteInfos.push_back(WriteInfo{
        .Binding = binding,
        .Type    = WriteType::UniformBuffer,
        .InfoId  = mBufferInfos.size() - 1,
    });

    return *this;
}

DescriptorUpdater &DescriptorUpdater::WriteShaderStorageBuffer(uint32_t     binding,
                                                               VkBuffer     buffer,
                                                               VkDeviceSize size)
{
    auto &bufferInfo  = mBufferInfos.emplace_back();
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = size;

    mWriteInfos.push_back(WriteInfo{
        .Binding = binding,
        .Type    = WriteType::ShaderStorageBuffer,
        .InfoId  = mBufferInfos.size() - 1,
    });

    return *this;
}

DescriptorUpdater &DescriptorUpdater::WriteImageSampler(uint32_t    binding,
                                                        VkImageView imageView,
                                                        VkSampler   sampler)
{
    auto &imageInfo       = mImageInfos.emplace_back();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = imageView;
    imageInfo.sampler     = sampler;

    mWriteInfos.push_back(WriteInfo{
        .Binding = binding,
        .Type    = WriteType::CombinedImageSampler,
        .InfoId  = mImageInfos.size() - 1,
    });

    return *this;
}

DescriptorUpdater &DescriptorUpdater::WriteImageStorage(uint32_t    binding,
                                                        VkImageView imageView)
{
    auto &imageInfo       = mImageInfos.emplace_back();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView   = imageView;

    mWriteInfos.push_back(WriteInfo{
        .Binding = binding,
        .Type    = WriteType::StorageImage,
        .InfoId  = mImageInfos.size() - 1,
    });

    return *this;
}

void DescriptorUpdater::Update(VulkanContext &ctx)
{
    std::vector<VkWriteDescriptorSet> writes;

    for (auto &writeInfo : mWriteInfos)
    {
        auto &write = writes.emplace_back();

        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = mDescriptorSet;
        write.dstBinding      = writeInfo.Binding;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;

        switch (writeInfo.Type)
        {
        case WriteType::UniformBuffer: {
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo    = &mBufferInfos[writeInfo.InfoId];
            break;
        }
        case WriteType::ShaderStorageBuffer: {
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo    = &mBufferInfos[writeInfo.InfoId];
            break;
        }
        case WriteType::CombinedImageSampler: {
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo     = &mImageInfos[writeInfo.InfoId];
            break;
        }
        case WriteType::StorageImage: {
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write.pImageInfo     = &mImageInfos[writeInfo.InfoId];
            break;
        }
        }
    }

    vkUpdateDescriptorSets(ctx.Device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

DynamicDescriptorAllocator::DynamicDescriptorAllocator(VulkanContext &ctx) : mCtx(ctx)
{
}

void DynamicDescriptorAllocator::OnInit(std::span<VkDescriptorPoolSize> sizes)
{
    mCountsPerSet.assign(sizes.begin(), sizes.end());

    mReadyPools.push_back(CreatePool());
    GrowSetsPerPool();
}

VkDescriptorSet DynamicDescriptorAllocator::Allocate(VkDescriptorSetLayout &layout)
{
    VkDescriptorPool pool = GetPool();

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext                       = nullptr;
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &layout;

    VkDescriptorSet set;
    VkResult        res = vkAllocateDescriptorSets(mCtx.Device, &allocInfo, &set);

    if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL)
    {
        mFullPools.push_back(pool);

        pool                     = GetPool();
        allocInfo.descriptorPool = pool;

        auto ret = vkAllocateDescriptorSets(mCtx.Device, &allocInfo, &set);

        vassert(ret == VK_SUCCESS, "Failed to allocate descriptor sets!");
    }

    mReadyPools.push_back(pool);

    return set;
}

std::vector<VkDescriptorSet> DynamicDescriptorAllocator::Allocate(
    std::span<VkDescriptorSetLayout> layouts)
{
    VkDescriptorPool pool = GetPool();

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext                       = nullptr;
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts        = layouts.data();

    std::vector<VkDescriptorSet> sets(layouts.size());
    VkResult res = vkAllocateDescriptorSets(mCtx.Device, &allocInfo, sets.data());

    if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL)
    {
        mFullPools.push_back(pool);

        pool                     = GetPool();
        allocInfo.descriptorPool = pool;

        auto ret = vkAllocateDescriptorSets(mCtx.Device, &allocInfo, sets.data());

        vassert(ret == VK_SUCCESS, "Failed to allocate descriptor sets!");
    }

    mReadyPools.push_back(pool);

    return sets;
}

VkDescriptorPool DynamicDescriptorAllocator::GetPool()
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

VkDescriptorPool DynamicDescriptorAllocator::CreatePool()
{
    std::vector<VkDescriptorPoolSize> poolSizes = mCountsPerSet;

    for (auto &elem : poolSizes)
        elem.descriptorCount *= mSetsPerPool;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags                      = 0;
    poolInfo.maxSets                    = mSetsPerPool;
    poolInfo.poolSizeCount              = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes                 = poolSizes.data();

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(mCtx.Device, &poolInfo, nullptr, &newPool);

    return newPool;
}

void DynamicDescriptorAllocator::GrowSetsPerPool()
{
    constexpr double   growthFactor   = 1.5f;
    constexpr uint32_t maxSetsPerPool = 4096;

    mSetsPerPool = static_cast<uint32_t>(growthFactor * mSetsPerPool);
    mSetsPerPool = std::min(mSetsPerPool, maxSetsPerPool);
}

void DynamicDescriptorAllocator::ResetPools()
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

void DynamicDescriptorAllocator::DestroyPools()
{
    for (auto pool : mReadyPools)
        vkDestroyDescriptorPool(mCtx.Device, pool, nullptr);

    for (auto pool : mFullPools)
        vkDestroyDescriptorPool(mCtx.Device, pool, nullptr);

    mReadyPools.clear();
    mFullPools.clear();
}