#pragma once

#include "DeletionQueue.h"
#include "VulkanContext.h"

#include "volk.h"

#include <span>
#include <string>

class DescriptorSetLayoutBuilder {
  public:
    DescriptorSetLayoutBuilder(std::string_view debugName);

    DescriptorSetLayoutBuilder &AddUniformBuffer(uint32_t binding, uint32_t stages);
    DescriptorSetLayoutBuilder &AddStorageBuffer(uint32_t binding, uint32_t stages);
    DescriptorSetLayoutBuilder &AddCombinedSampler(uint32_t binding, uint32_t stages);
    DescriptorSetLayoutBuilder &AddStorageImage(uint32_t binding, uint32_t stages);

    using PoolSizes = std::array<VkDescriptorPoolSize, 4>;
    using Result    = std::pair<VkDescriptorSetLayout, PoolSizes>;

    Result Build(VulkanContext &ctx);
    Result Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    DescriptorSetLayoutBuilder &AddBinding(uint32_t binding, VkDescriptorType type,
                                           uint32_t stages);

    VkDescriptorSetLayout BuildLayout(VulkanContext &ctx);
    PoolSizes             BuildSizes();

  private:
    std::vector<VkDescriptorSetLayoutBinding> mBindings;
    std::string                               mDebugName;

    struct {
        uint32_t StorageImage         = 0;
        uint32_t CombinedImageSampler = 0;
        uint32_t UniformBuffer        = 0;
        uint32_t StorageBuffer        = 0;
    } mBindingCounts;
};

namespace Descriptor
{
VkDescriptorPool InitPool(VulkanContext &ctx, uint32_t maxSets,
                          std::span<VkDescriptorPoolSize> poolSizes);

VkDescriptorPool InitPool(VulkanContext &ctx, uint32_t maxSets,
                          std::span<VkDescriptorPoolSize> poolSizes,
                          DeletionQueue                  &deletionQueue);

VkDescriptorSet Allocate(VulkanContext &ctx, VkDescriptorPool pool,
                         VkDescriptorSetLayout &layout);

std::vector<VkDescriptorSet> Allocate(VulkanContext &ctx, VkDescriptorPool pool,
                                      std::span<VkDescriptorSetLayout> layouts);
}; // namespace Descriptor

class DescriptorUpdater {
  public:
    DescriptorUpdater(VkDescriptorSet descriptorSet);

    // TODO: add support for different buffers

    /// Assumes no offset into the buffer
    DescriptorUpdater &WriteUniformBuffer(uint32_t binding, VkBuffer buffer,
                                          VkDeviceSize size);

    DescriptorUpdater &WriteStorageBuffer(uint32_t binding, VkBuffer buffer,
                                          VkDeviceSize size);

    /// Uses combined sampler, with read only optimal layout
    DescriptorUpdater &WriteCombinedSampler(uint32_t binding, VkImageView imageView,
                                            VkSampler sampler);

    DescriptorUpdater &WriteStorageImage(uint32_t binding, VkImageView imageView);

    void Update(VulkanContext &ctx);

  private:
    enum class WriteType
    {
        UniformBuffer,
        ShaderStorageBuffer,
        CombinedImageSampler,
        StorageImage,
    };

    struct WriteInfo {
        uint32_t  Binding;
        WriteType Type;
        size_t    InfoId;
    };

    std::vector<VkDescriptorBufferInfo> mBufferInfos;
    std::vector<VkDescriptorImageInfo>  mImageInfos;

    std::vector<WriteInfo> mWriteInfos;

    VkDescriptorSet mDescriptorSet;
};

// Based on growable descriptor allocator from:
// https://vkguide.dev/docs/new_chapter_4/descriptor_abstractions/
class DynamicDescriptorAllocator {
  public:
    DynamicDescriptorAllocator(VulkanContext &ctx);
    ~DynamicDescriptorAllocator();

    void OnInit(std::span<VkDescriptorPoolSize> sizes);

    VkDescriptorSet              Allocate(VkDescriptorSetLayout &layout);
    std::vector<VkDescriptorSet> Allocate(std::span<VkDescriptorSetLayout> layouts);

    void ResetPools();
    void DestroyPools();

  private:
    VkDescriptorPool GetPool();
    VkDescriptorPool CreatePool();

    void GrowSetsPerPool();

  private:
    VulkanContext &mCtx;

    std::vector<VkDescriptorPool> mReadyPools;
    std::vector<VkDescriptorPool> mFullPools;

    uint32_t                          mSetsPerPool = 32;
    std::vector<VkDescriptorPoolSize> mCountsPerSet;
};