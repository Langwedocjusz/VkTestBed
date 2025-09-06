#pragma once

#include "DeletionQueue.h"
#include "VertexLayout.h"
#include "VulkanContext.h"

#include <vulkan/vulkan.h>

#include <optional>
#include <set>
#include <string_view>
#include <vector>

struct Pipeline {
    VkPipeline Handle;
    VkPipelineLayout Layout;
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

    PipelineBuilder &EnableDepthTest(VkCompareOp compareOp = VK_COMPARE_OP_LESS);
    PipelineBuilder &EnableBlending();

    PipelineBuilder &AddDescriptorSetLayout(VkDescriptorSetLayout descriptor);

    PipelineBuilder &SetPushConstantSize(uint32_t size);

    Pipeline Build(VulkanContext &ctx);
    Pipeline Build(VulkanContext &ctx, DeletionQueue &queue);

  private:
    Pipeline BuildImpl(VulkanContext &ctx);
    void UpdateVertexInput();

  private:
    std::optional<std::string> mVertexPath = std::nullopt;
    std::optional<std::string> mFragmentPath = std::nullopt;

    std::set<VkDynamicState> mDynamicStates;

    VkPipelineVertexInputStateCreateInfo mVertexInput;
    VkPipelineInputAssemblyStateCreateInfo mInputAssembly;
    VkPipelineRasterizationStateCreateInfo mRaster;
    VkPipelineMultisampleStateCreateInfo mMultisample;
    VkPipelineColorBlendAttachmentState mColorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo mColorBlend;
    VkPipelineDepthStencilStateCreateInfo mDepthStencil;

    std::optional<VkFormat> mColorFormat = std::nullopt;
    std::optional<VkFormat> mDepthFormat = std::nullopt;

    std::vector<VkDescriptorSetLayout> mDescriptorLayouts;

    std::optional<VkPushConstantRange> mPushConstantRange = std::nullopt;

    VkVertexInputBindingDescription mBindingDescription;
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
    std::optional<std::string> mShaderPath;
    std::vector<VkDescriptorSetLayout> mDescriptorLayouts;
    std::optional<VkPushConstantRange> mPushConstantRange;
    std::string mDebugName;
};