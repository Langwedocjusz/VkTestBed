#include "Descriptor.h"
#include "CppUtils.h"
#include "Pch.h"

#include "Vassert.h"
#include "VkUtils.h"

#include "volk.h"

#include <cstdint>
#include <ranges>
#include <vulkan/vulkan_core.h>

static std::vector<VkDescriptorPoolSize> GetRawPoolSizes(Descriptor::PoolCounts &counts)
{
    std::vector<VkDescriptorPoolSize> ret{};

    // It is invalid usage to pass descriptor pool a count
    // with zero of something. Hence we must manually
    // sieve the values:

    if (counts.StorageImage > 0)
        ret.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, counts.StorageImage});
    if (counts.CombinedImageSampler > 0)
        ret.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, counts.CombinedImageSampler});
    if (counts.UniformBuffer > 0)
        ret.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, counts.UniformBuffer});
    if (counts.StorageBuffer > 0)
        ret.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, counts.StorageBuffer});

    return ret;
}

// === Layout Builder Implementation: ===============================================

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(std::string_view debugName)
    : mDebugName(debugName)
{
}

DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::AddBinding(
    uint32_t binding, VkDescriptorType type, uint32_t stages, uint32_t count
)
{
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding         = binding;
    layoutBinding.descriptorType  = type;
    layoutBinding.stageFlags      = stages;
    layoutBinding.descriptorCount = count;

    // Hardcoded for now:
    layoutBinding.pImmutableSamplers = nullptr;

    mBindings.push_back(layoutBinding);

    return *this;
}

DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::AddUniformBuffer(
    uint32_t binding, uint32_t stages, uint32_t count
)
{
    return AddBinding(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stages, count);
}

DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::AddStorageBuffer(
    uint32_t binding, uint32_t stages, uint32_t count
)
{
    return AddBinding(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages, count);
}

DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::AddCombinedSampler(
    uint32_t binding, uint32_t stages, uint32_t count
)
{
    return AddBinding(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stages, count);
}

DescriptorSetLayoutBuilder &DescriptorSetLayoutBuilder::AddStorageImage(
    uint32_t binding, uint32_t stages, uint32_t count
)
{
    return AddBinding(binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stages, count);
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::Build(VulkanContext &ctx)
{
    return Descriptor::BuildLayout(ctx, mBindings, mDebugName);
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::Build(VulkanContext &ctx, DeletionQueue &queue)
{
    const auto layout = Descriptor::BuildLayout(ctx, mBindings, mDebugName);
    queue.push_back(layout);

    return layout;
}

// === General Descriptor Utilities Implementation: ===============================================

VkDescriptorSetLayout Descriptor::BuildLayout(
    VulkanContext &ctx, const std::vector<VkDescriptorSetLayoutBinding> &bindings, const std::string &name
)
{
    VkDescriptorSetLayout layout{};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    auto ret = vkCreateDescriptorSetLayout(ctx.Device, &layoutInfo, nullptr, &layout);

    vassert(ret == VK_SUCCESS, "Failed to create descriptor set layout!");

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, layout, name);

    return layout;
}

VkDescriptorPool Descriptor::InitPool(VulkanContext &ctx, PoolCounts counts)
{
    VkDescriptorPool pool{};

    auto poolSizes = GetRawPoolSizes(counts);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = counts.DescriptorSet;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();

    auto ret = vkCreateDescriptorPool(ctx.Device, &poolInfo, nullptr, &pool);

    vassert(ret == VK_SUCCESS, "Failed to create descriptor pool!");

    return pool;
}

VkDescriptorPool Descriptor::InitPool(VulkanContext &ctx, PoolCounts counts, DeletionQueue &deletionQueue)
{
    VkDescriptorPool pool = InitPool(ctx, counts);
    deletionQueue.push_back(pool);
    return pool;
}

VkDescriptorSet Descriptor::Allocate(VulkanContext &ctx, VkDescriptorPool pool, VkDescriptorSetLayout &layout)
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
    VulkanContext &ctx, VkDescriptorPool pool, std::span<VkDescriptorSetLayout> layouts
)
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

void Descriptor::Update(VulkanContext &ctx, VkDescriptorSet set, std::vector<WriteInfo> &&writeInfos)
{
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo>  imageInfos;
    std::vector<VkWriteDescriptorSet>   writes;

    for (const auto &info : writeInfos)
    {
        auto &write = writes.emplace_back();

        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = set;
        write.dstBinding      = info.Binding;
        write.dstArrayElement = 0;

        std::visit(overloaded{
            [&](WriteUniformBuffers arg) {
                const size_t startIdx = bufferInfos.size();

                for (auto &buffer : arg.Buffers)
                {
                    bufferInfos.push_back({
                        .buffer = buffer.Handle,
                        .offset = 0,
                        .range  = buffer.Size,
                     });
                }

                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                write.pBufferInfo = &bufferInfos[startIdx];
                write.descriptorCount = static_cast<uint32_t>(arg.Buffers.size());
            },
            [&](WriteStorageBuffers arg){
                const size_t startIdx = bufferInfos.size();

                for (auto &buffer : arg.Buffers)
                {
                    bufferInfos.push_back({
                        .buffer = buffer.Handle,
                        .offset = 0,
                        .range  = buffer.Size,
                     });
                }

                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                write.pBufferInfo = &bufferInfos[startIdx];
                write.descriptorCount = static_cast<uint32_t>(arg.Buffers.size());
            },
            [&](WriteCombinedSamplers arg){
                const size_t startIdx = imageInfos.size();

                for (auto view : arg.Views)
                {
                    imageInfos.push_back({
                        .sampler     = arg.Sampler,
                        .imageView   = view,
                        .imageLayout = arg.Layout
                    });
                }

                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.pImageInfo     = &imageInfos[startIdx];
                write.descriptorCount = static_cast<uint32_t>(arg.Views.size());
            },
            [&](WriteStorageImages arg){
                const size_t startIdx = imageInfos.size();

                for (auto view : arg.Views)
                {
                    imageInfos.push_back({
                        .sampler     = {},
                        .imageView   = view,
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                    });
                }

                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                write.pImageInfo     = &imageInfos[startIdx];
                write.descriptorCount = static_cast<uint32_t>(arg.Views.size());
            },
        }, info.Data);
    }

    vkUpdateDescriptorSets(ctx.Device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

// === Growable Allocator Implementation: ===============================================

GrowableDescriptorAllocator::GrowableDescriptorAllocator(VulkanContext &ctx)
    : mCtx(ctx)
{
}

GrowableDescriptorAllocator::~GrowableDescriptorAllocator()
{
    bool init = !mCountsPerSet.empty();

    if (init)
        DestroyPools();
}

void GrowableDescriptorAllocator::OnInit(Descriptor::PoolCounts counts)
{
    auto sizes = GetRawPoolSizes(counts);

    mCountsPerSet.assign(sizes.begin(), sizes.end());

    mReadyPools.push_back(CreatePool());
    GrowSetsPerPool();
}

VkDescriptorSet GrowableDescriptorAllocator::Allocate(VkDescriptorSetLayout &layout)
{
    VkDescriptorPool pool = GetPool();

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext                       = nullptr;
    allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool              = pool;
    allocInfo.descriptorSetCount          = 1;
    allocInfo.pSetLayouts                 = &layout;

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

std::vector<VkDescriptorSet> GrowableDescriptorAllocator::Allocate(std::span<VkDescriptorSetLayout> layouts)
{
    VkDescriptorPool pool = GetPool();

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext                       = nullptr;
    allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool              = pool;
    allocInfo.descriptorSetCount          = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts                 = layouts.data();

    std::vector<VkDescriptorSet> sets(layouts.size());
    VkResult                     res = vkAllocateDescriptorSets(mCtx.Device, &allocInfo, sets.data());

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

VkDescriptorPool GrowableDescriptorAllocator::GetPool()
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

VkDescriptorPool GrowableDescriptorAllocator::CreatePool()
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

void GrowableDescriptorAllocator::GrowSetsPerPool()
{
    constexpr double   growthFactor   = 1.5f;
    constexpr uint32_t maxSetsPerPool = 4096;

    mSetsPerPool = static_cast<uint32_t>(growthFactor * mSetsPerPool);
    mSetsPerPool = std::min(mSetsPerPool, maxSetsPerPool);
}

void GrowableDescriptorAllocator::ResetPools()
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

void GrowableDescriptorAllocator::DestroyPools()
{
    for (auto pool : mReadyPools)
        vkDestroyDescriptorPool(mCtx.Device, pool, nullptr);

    for (auto pool : mFullPools)
        vkDestroyDescriptorPool(mCtx.Device, pool, nullptr);

    mReadyPools.clear();
    mFullPools.clear();
}