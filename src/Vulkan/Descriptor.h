#pragma once

#include "DeletionQueue.h"
#include "VulkanContext.h"

#include <vulkan/vulkan.h>

#include <span>
#include <string>

class DescriptorSetLayoutBuilder {
  public:
    DescriptorSetLayoutBuilder(std::string_view debugName);

    DescriptorSetLayoutBuilder &AddBinding(uint32_t binding, VkDescriptorType type,
                                           uint32_t stages);

    VkDescriptorSetLayout Build(VulkanContext &ctx);
    VkDescriptorSetLayout Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    VkDescriptorSetLayout BuildImpl(VulkanContext &ctx);

  private:
    std::vector<VkDescriptorSetLayoutBinding> mBindings;
    std::string mDebugName;
};

namespace Descriptor
{
VkDescriptorPool InitPool(VulkanContext &ctx, uint32_t maxSets,
                          std::span<VkDescriptorPoolSize> poolSizes);

VkDescriptorSet Allocate(VulkanContext &ctx, VkDescriptorPool pool,
                         VkDescriptorSetLayout &layout);

std::vector<VkDescriptorSet> Allocate(VulkanContext &ctx, VkDescriptorPool pool,
                                      std::span<VkDescriptorSetLayout> layouts);
}; // namespace Descriptor

class DescriptorUpdater {
  public:
    DescriptorUpdater(VkDescriptorSet descriptorSet);

    // To-do: add support for different buffers

    /// Assumes no offset into the buffer
    DescriptorUpdater &WriteUniformBuffer(uint32_t binding, VkBuffer buffer,
                                          VkDeviceSize size);

    DescriptorUpdater &WriteShaderStorageBuffer(uint32_t binding, VkBuffer buffer,
                                                VkDeviceSize size);

    /// Uses combined sampler, with read only optimal layout
    DescriptorUpdater &WriteImageSampler(uint32_t binding, VkImageView imageView,
                                         VkSampler sampler);

    DescriptorUpdater &WriteImageStorage(uint32_t binding, VkImageView imageView);

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
        uint32_t Binding;
        WriteType Type;
        size_t InfoId;
    };

    std::vector<VkDescriptorBufferInfo> mBufferInfos;
    std::vector<VkDescriptorImageInfo> mImageInfos;

    std::vector<WriteInfo> mWriteInfos;

    VkDescriptorSet mDescriptorSet;
};

// Based on growable descriptor allocator from:
// https://vkguide.dev/docs/new_chapter_4/descriptor_abstractions/
class DescriptorAllocator {
  public:
    DescriptorAllocator(VulkanContext &ctx);
    void OnInit(std::span<VkDescriptorPoolSize> sizes);

    VkDescriptorSet Allocate(VkDescriptorSetLayout &layout);
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

    uint32_t mSetsPerPool = 32;
    std::vector<VkDescriptorPoolSize> mCountsPerSet;
};