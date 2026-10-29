#pragma once

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

struct PipelineLayoutInfo {
    std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> DescriptorBindings;
    uint32_t                                                      PushConstantSize = 0;

    // friend bool operator==(const PipelineLayoutInfo &lhs, const PipelineLayoutInfo &rhs);
};

// TODO: There is a lot of code duplication between two pipeline types.
// This could be easily refactored using a base class templated on the two constants.
// But that would require pulling the implementation code into the header.
// Need to think about this...

class GraphicsPipeline {
  public:
    GraphicsPipeline(VulkanContext &ctx);
    ~GraphicsPipeline();
    GraphicsPipeline(const GraphicsPipeline &)            = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;
    GraphicsPipeline(GraphicsPipeline &&) noexcept;
    GraphicsPipeline &operator=(GraphicsPipeline &&) noexcept;

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

    VkPipeline         Handle     = VK_NULL_HANDLE;
    VkPipelineLayout   Layout     = VK_NULL_HANDLE;
    PipelineLayoutInfo LayoutInfo = {};

    std::map<uint32_t, VkDescriptorSetLayout> DescriptorLayouts;
    std::map<uint32_t, VkDescriptorSet>       DescriptorSets;

  private:
    VulkanContext &mCtx;
};

class ComputePipeline {
  public:
    ComputePipeline(VulkanContext &ctx);
    ~ComputePipeline();
    ComputePipeline(const ComputePipeline &)            = delete;
    ComputePipeline &operator=(const ComputePipeline &) = delete;
    ComputePipeline(ComputePipeline &&) noexcept;
    ComputePipeline &operator=(ComputePipeline &&) noexcept;

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

    VkPipeline         Handle     = VK_NULL_HANDLE;
    VkPipelineLayout   Layout     = VK_NULL_HANDLE;
    PipelineLayoutInfo LayoutInfo = {};

    std::map<uint32_t, VkDescriptorSetLayout> DescriptorLayouts;
    std::map<uint32_t, VkDescriptorSet>       DescriptorSets;

    std::array<uint32_t, 3> LocalSize = {0, 0, 0};

  private:
    VulkanContext &mCtx;
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

    void Build(VulkanContext &ctx);

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

    void Build(VulkanContext &ctx);

  private:
    ComputePipeline &mOutput;

    std::optional<std::string>                mShaderPath;
    std::map<uint32_t, VkDescriptorSetLayout> mDescriptorOverrides;
    std::string                               mDebugName;
};