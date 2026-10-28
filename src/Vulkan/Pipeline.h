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

// The structure of this module is a litle unusual.
// The builders do not return by value and instead take a reference outparameter.
// This is because when supporting hot-reload and reflection the lifecycle of a Pipeline is a bit peculiar.
// On the one hand the raw VkPipeline part needs to be rebuilt when shader code changes.
// On the other hand its layout (descriptor set binding, push constants) should remain consistent,
// as changing it would require change in the host-side cpu code that feeds the graphics API with data.
// To reflect that we only want to build the reflected descriptor sets once, and later only
// rebuild the pipeline itself when layout still matches.

class GraphicsPipeline {
  public:
    void Bind(VkCommandBuffer cmd);

    void AllocateReflectedSets(VulkanContext &ctx, VkDescriptorPool pool);
    void BindReflectedSets(VkCommandBuffer cmd);

    void BindDescriptorSet(VkCommandBuffer cmd, VkDescriptorSet set, uint32_t setIdx);
    void BindDescriptorSets(VkCommandBuffer cmd, std::span<VkDescriptorSet> sets, uint32_t startIdx);

    template <typename T>
    void PushConstants(VkCommandBuffer cmd, const T &data)
    {
        vkCmdPushConstants(cmd, Layout, PCStage, 0, sizeof(data), &data);
    }

  public:
    static constexpr VkPipelineBindPoint BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    static constexpr VkShaderStageFlags  PCStage   = VK_SHADER_STAGE_ALL_GRAPHICS;

    VkPipeline       Handle;
    VkPipelineLayout Layout;

    std::map<uint32_t, VkDescriptorSetLayout> Layouts;
    std::map<uint32_t, VkDescriptorSet>       Sets;
};

class ComputePipeline {
  public:
    void Bind(VkCommandBuffer cmd);

    void AllocateReflectedSets(VulkanContext &ctx, VkDescriptorPool pool);
    void BindReflectedSets(VkCommandBuffer cmd);

    void BindDescriptorSet(VkCommandBuffer cmd, VkDescriptorSet set, uint32_t setIdx);
    void BindDescriptorSets(VkCommandBuffer cmd, std::span<VkDescriptorSet> sets, uint32_t startIdx);

    template <typename T>
    void PushConstants(VkCommandBuffer cmd, const T &data)
    {
        vkCmdPushConstants(cmd, Layout, PCStage, 0, sizeof(data), &data);
    }

    void Dispatch(VkCommandBuffer cmd, uint32_t sizeX = 1, uint32_t sizeY = 1, uint32_t sizeZ = 1);

  public:
    static constexpr VkPipelineBindPoint BindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    static constexpr VkShaderStageFlags  PCStage   = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipeline              Handle;
    VkPipelineLayout        Layout;
    std::array<uint32_t, 3> LocalSize;

    std::map<uint32_t, VkDescriptorSetLayout> Layouts;
    std::map<uint32_t, VkDescriptorSet>       Sets;
};

class GraphicsPipelineBuilder {
  public:
    GraphicsPipelineBuilder(GraphicsPipeline &output);

    GraphicsPipelineBuilder &SetDebugName(std::string_view debugName);

    GraphicsPipelineBuilder &SetShaderPathVertex(std::string_view path);
    GraphicsPipelineBuilder &SetShaderPathFragment(std::string_view path);
    
    GraphicsPipelineBuilder &OverrideDescriptor(uint32_t binding, VkDescriptorSetLayout layout);

    /// If vertex input is not set, vertex data can't be accessed the usual way
    /// in vertex shaders. This is actually the desired behaviour when doing
    /// vertex pulling or generating vertices on-the-fly in the shader itself.
    GraphicsPipelineBuilder &SetVertexInput(
        const Vertex::Layout &layout, uint32_t binding, VkVertexInputRate inputRate
    );

    GraphicsPipelineBuilder &SetTopology(VkPrimitiveTopology topo);
    GraphicsPipelineBuilder &SetPolygonMode(VkPolygonMode mode);
    GraphicsPipelineBuilder &SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);

    GraphicsPipelineBuilder &RequestDynamicState(VkDynamicState state);

    GraphicsPipelineBuilder &SetColorFormat(VkFormat format);
    GraphicsPipelineBuilder &SetDepthFormat(VkFormat format);
    GraphicsPipelineBuilder &SetStencilFormat(VkFormat format);

    GraphicsPipelineBuilder &EnableDepthTest(VkCompareOp compareOp = VK_COMPARE_OP_LESS);
    GraphicsPipelineBuilder &EnableDepthTestNoWrite(VkCompareOp compareOp = VK_COMPARE_OP_LESS);
    GraphicsPipelineBuilder &EnableStencilTest(VkStencilOpState front, VkStencilOpState back);
    GraphicsPipelineBuilder &EnableBlending();

    GraphicsPipelineBuilder &SetMultisampling(VkSampleCountFlagBits sampleCount);

    GraphicsPipeline Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    GraphicsPipeline BuildImpl(VulkanContext &ctx);

  private:
    GraphicsPipeline &mOutput;

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

    std::map<uint32_t, VkDescriptorSetLayout> mDescriptorOverrides;

    VkVertexInputBindingDescription                mBindingDescription;
    std::vector<VkVertexInputAttributeDescription> mAttributeDescriptions;

    std::string mDebugName;
};

class ComputePipelineBuilder {
  public:
    ComputePipelineBuilder(ComputePipeline &output);

    ComputePipelineBuilder &SetDebugName(std::string_view debugName);

    ComputePipelineBuilder &SetShaderPath(std::string_view path);

    ComputePipelineBuilder &OverrideDescriptor(uint32_t binding, VkDescriptorSetLayout layout);

    ComputePipeline Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    ComputePipeline BuildImpl(VulkanContext &ctx);

  private:
    ComputePipeline &mOutput;

    std::optional<std::string>                mShaderPath;
    std::map<uint32_t, VkDescriptorSetLayout> mDescriptorOverrides;
    std::string                               mDebugName;
};