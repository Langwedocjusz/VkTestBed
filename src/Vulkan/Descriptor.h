#pragma once

#include "Buffer.h"
#include "DeletionQueue.h"
#include "VulkanContext.h"

#include "volk.h"

#include <span>
#include <string>

class DescriptorSetLayoutBuilder {
  public:
    DescriptorSetLayoutBuilder(std::string_view debugName);

    DescriptorSetLayoutBuilder &AddUniformBuffer(uint32_t binding, uint32_t stages, uint32_t count = 1);
    DescriptorSetLayoutBuilder &AddStorageBuffer(uint32_t binding, uint32_t stages, uint32_t count = 1);
    DescriptorSetLayoutBuilder &AddCombinedSampler(uint32_t binding, uint32_t stages, uint32_t count = 1);
    DescriptorSetLayoutBuilder &AddStorageImage(uint32_t binding, uint32_t stages, uint32_t count = 1);

    VkDescriptorSetLayout Build(VulkanContext &ctx);
    VkDescriptorSetLayout Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    DescriptorSetLayoutBuilder &AddBinding(uint32_t binding, VkDescriptorType type, uint32_t stages, uint32_t count);

  private:
    std::vector<VkDescriptorSetLayoutBinding> mBindings;
    std::string                               mDebugName;
};

namespace Descriptor
{
struct PoolCounts {
    uint32_t DescriptorSet        = 0;
    uint32_t StorageImage         = 0;
    uint32_t CombinedImageSampler = 0;
    uint32_t UniformBuffer        = 0;
    uint32_t StorageBuffer        = 0;
};

VkDescriptorSetLayout BuildLayout(
    VulkanContext &ctx, const std::vector<VkDescriptorSetLayoutBinding> &bindings, const std::string &name
);

VkDescriptorPool InitPool(VulkanContext &ctx, PoolCounts counts);
VkDescriptorPool InitPool(VulkanContext &ctx, PoolCounts counts, DeletionQueue &deletionQueue);

VkDescriptorSet              Allocate(VulkanContext &ctx, VkDescriptorPool pool, VkDescriptorSetLayout &layout);
std::vector<VkDescriptorSet> Allocate(
    VulkanContext &ctx, VkDescriptorPool pool, std::span<VkDescriptorSetLayout> layouts
);

namespace Write
{
// clang-format off
struct UniformBuffers {
    UniformBuffers(std::span<Buffer> buffers) : Buffers(buffers) {}
    UniformBuffers(Buffer &buffer) : Buffers(std::span(&buffer, 1)) {}

    std::span<Buffer> Buffers;
};

struct StorageBuffers {
    StorageBuffers(std::span<Buffer> buffers) : Buffers(buffers) {}
    StorageBuffers(Buffer &buffer) : Buffers(std::span(&buffer, 1)) {}

    std::span<Buffer> Buffers;
};

struct CombinedSamplers {
    CombinedSamplers(VkImageView &view, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        : Views(std::span(&view, 1)), Sampler(sampler), Layout(layout)
    {}
    CombinedSamplers(std::span<VkImageView> views, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        : Views(views), Sampler(sampler), Layout(layout)
    {}

    std::span<VkImageView> Views;
    VkSampler              Sampler;
    VkImageLayout          Layout;
};

struct StorageImages {
    StorageImages(VkImageView &view): Views(std::span(&view, 1)) {}
    StorageImages(std::span<VkImageView> views) : Views(views) {}

    std::span<VkImageView> Views;
};
// clang-format on
}; // namespace Write

using WriteData =
    std::variant<Write::UniformBuffers, Write::StorageBuffers, Write::CombinedSamplers, Write::StorageImages>;

struct WriteInfo {
    uint32_t  Binding;
    WriteData Data;
};

void Update(VulkanContext &ctx, VkDescriptorSet set, std::initializer_list<WriteInfo> writeInfos);
}; // namespace Descriptor

// Based on growable descriptor allocator from:
// https://vkguide.dev/docs/new_chapter_4/descriptor_abstractions/
class GrowableDescriptorAllocator {
  public:
    GrowableDescriptorAllocator(VulkanContext &ctx);
    ~GrowableDescriptorAllocator();

    // TODO: Why is there 2-phase initialziation here?...
    void OnInit(Descriptor::PoolCounts counts);

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