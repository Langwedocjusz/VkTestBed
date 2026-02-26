#pragma once

#include "DeletionQueue.h"
#include "VertexLayout.h"
#include "VulkanContext.h"

#include "volk.h"

#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <vector>

class Pipeline {
  public:
    Pipeline() = default;
    static Pipeline MakePipeline(VkPipelineBindPoint, VkShaderStageFlags pcStageFlags);

    void Bind(VkCommandBuffer cmd);

    void BindDescriptorSet(VkCommandBuffer cmd, VkDescriptorSet set, uint32_t setIdx);
    void BindDescriptorSets(VkCommandBuffer cmd, std::span<VkDescriptorSet> sets,
                            uint32_t startIdx);

    template<typename T>
    void PushConstants(VkCommandBuffer cmd, T& data)
    {
        vkCmdPushConstants(cmd, Layout, mPCStageFlags, 0, sizeof(data), &data);
    }

  public:
    VkPipeline       Handle;
    VkPipelineLayout Layout;

  private:
    VkPipelineBindPoint mBindPoint;
    VkShaderStageFlags mPCStageFlags;
};

class PipelineBuilder {
  public:
    PipelineBuilder(std::string_view debugName);

    PipelineBuilder &SetShaderPathVertex(std::string_view path);
    PipelineBuilder &SetShaderPathFragment(std::string_view path);

    /// If vertex input is not set, vertex data can't be accessed the usual way
    /// in vertex shaders. This is actually the desired behaviour when doing
    /// vertex pulling or generating vertices on-the-fly in the shader itself.
    PipelineBuilder &SetVertexInput(const Vertex::Layout &layout, uint32_t binding,
                                    VkVertexInputRate inputRate);

    PipelineBuilder &SetTopology(VkPrimitiveTopology topo);
    PipelineBuilder &SetPolygonMode(VkPolygonMode mode);
    PipelineBuilder &SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);

    PipelineBuilder &RequestDynamicState(VkDynamicState state);

    PipelineBuilder &SetColorFormat(VkFormat format);
    PipelineBuilder &SetDepthFormat(VkFormat format);
    PipelineBuilder &SetStencilFormat(VkFormat format);

    PipelineBuilder &EnableDepthTest(VkCompareOp compareOp = VK_COMPARE_OP_LESS);
    PipelineBuilder &EnableDepthTestNoWrite(VkCompareOp compareOp = VK_COMPARE_OP_LESS);
    PipelineBuilder &EnableStencilTest(VkStencilOpState front, VkStencilOpState back);
    PipelineBuilder &EnableBlending();

    PipelineBuilder &AddDescriptorSetLayout(VkDescriptorSetLayout descriptor);

    PipelineBuilder &SetPushConstantSize(uint32_t size);

    PipelineBuilder &SetMultisampling(VkSampleCountFlagBits sampleCount);

    Pipeline Build(VulkanContext &ctx);
    Pipeline Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    Pipeline BuildImpl(VulkanContext &ctx);
    void     UpdateVertexInput();

  private:
    std::optional<std::string> mVertexPath   = std::nullopt;
    std::optional<std::string> mFragmentPath = std::nullopt;

    std::set<VkDynamicState> mDynamicStates;

    VkPipelineVertexInputStateCreateInfo   mVertexInput;
    VkPipelineInputAssemblyStateCreateInfo mInputAssembly;
    VkPipelineRasterizationStateCreateInfo mRaster;
    VkPipelineMultisampleStateCreateInfo   mMultisample;
    VkPipelineColorBlendAttachmentState    mColorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo    mColorBlend;
    VkPipelineDepthStencilStateCreateInfo  mDepthStencil;

    std::optional<VkFormat> mColorFormat   = std::nullopt;
    std::optional<VkFormat> mDepthFormat   = std::nullopt;
    std::optional<VkFormat> mStencilFormat = std::nullopt;

    std::vector<VkDescriptorSetLayout> mDescriptorLayouts;

    std::optional<VkPushConstantRange> mPushConstantRange = std::nullopt;

    VkVertexInputBindingDescription                mBindingDescription;
    std::vector<VkVertexInputAttributeDescription> mAttributeDescriptions;

    std::string mDebugName;
};

class ComputePipelineBuilder {
  public:
    ComputePipelineBuilder(std::string_view debugName);

    ComputePipelineBuilder &SetShaderPath(std::string_view path);
    ComputePipelineBuilder &AddDescriptorSetLayout(VkDescriptorSetLayout descriptor);
    ComputePipelineBuilder &SetPushConstantSize(uint32_t size);

    Pipeline Build(VulkanContext &ctx);
    Pipeline Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    Pipeline BuildImpl(VulkanContext &ctx);

  private:
    std::optional<std::string>         mShaderPath;
    std::vector<VkDescriptorSetLayout> mDescriptorLayouts;
    std::optional<VkPushConstantRange> mPushConstantRange;
    std::string                        mDebugName;
};