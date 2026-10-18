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

class GraphicsPipeline{
public:
    void Bind(VkCommandBuffer cmd);

    void BindDescriptorSet(VkCommandBuffer cmd, VkDescriptorSet set, uint32_t setIdx);
    void BindDescriptorSets(VkCommandBuffer cmd, std::span<VkDescriptorSet> sets,
                            uint32_t startIdx);

    template <typename T>
    void PushConstants(VkCommandBuffer cmd, T &data)
    {
        vkCmdPushConstants(cmd, Layout, PCStage, 0, sizeof(data), &data);
    }

public:
    static constexpr VkPipelineBindPoint BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    static constexpr VkShaderStageFlags PCStage    = VK_SHADER_STAGE_ALL_GRAPHICS;

    VkPipeline       Handle;
    VkPipelineLayout Layout;
};

class ComputePipeline{
public:
    void Bind(VkCommandBuffer cmd);

    void BindDescriptorSet(VkCommandBuffer cmd, VkDescriptorSet set, uint32_t setIdx);
    void BindDescriptorSets(VkCommandBuffer cmd, std::span<VkDescriptorSet> sets,
                            uint32_t startIdx);

    template <typename T>
    void PushConstants(VkCommandBuffer cmd, T &data)
    {
        vkCmdPushConstants(cmd, Layout, PCStage, 0, sizeof(data), &data);
    }

    void Dispatch(VkCommandBuffer cmd, uint32_t sizeX = 1, uint32_t sizeY = 1, uint32_t sizeZ = 1);

public:
    static constexpr VkPipelineBindPoint BindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    static constexpr VkShaderStageFlags PCStage    = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipeline       Handle;
    VkPipelineLayout Layout;

    std::array<uint32_t, 3> LocalSize;
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

    PipelineBuilder &SetMultisampling(VkSampleCountFlagBits sampleCount);

    GraphicsPipeline Build(VulkanContext &ctx);
    GraphicsPipeline Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    GraphicsPipeline BuildImpl(VulkanContext &ctx);
    void             UpdateVertexInput();

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

    VkVertexInputBindingDescription                mBindingDescription;
    std::vector<VkVertexInputAttributeDescription> mAttributeDescriptions;

    std::string mDebugName;
};

class ComputePipelineBuilder {
  public:
    ComputePipelineBuilder(std::string_view debugName);

    ComputePipelineBuilder &SetShaderPath(std::string_view path);
    ComputePipelineBuilder &AddDescriptorSetLayout(VkDescriptorSetLayout descriptor);

    ComputePipeline Build(VulkanContext &ctx);
    ComputePipeline Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    ComputePipeline BuildImpl(VulkanContext &ctx);

  private:
    std::optional<std::string>         mShaderPath;
    std::vector<VkDescriptorSetLayout> mDescriptorLayouts;
    std::string                        mDebugName;
};