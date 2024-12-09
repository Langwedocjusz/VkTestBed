#pragma once

#include "VertexLayout.h"
#include "VulkanContext.h"

#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

struct Pipeline {
    VkPipeline Handle;
    VkPipelineLayout Layout;
};

class PipelineBuilder {
  public:
    PipelineBuilder();

    PipelineBuilder &SetShaderStages(
        const std::vector<VkPipelineShaderStageCreateInfo> &stages);

    /// If vertex input is not set, vertex data can't be accessed the usual way
    /// in vertex shaders. This is actually the desired behaviour when doing
    /// vertex pulling or generating vertices on-the-fly in the shader itself.
    PipelineBuilder &SetVertexInput(const Vertex::Layout &layout, uint32_t binding,
                                    VkVertexInputRate inputRate);

    PipelineBuilder &SetTopology(VkPrimitiveTopology topo);
    PipelineBuilder &SetPolygonMode(VkPolygonMode mode);
    PipelineBuilder &SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);

    PipelineBuilder &SetColorFormat(VkFormat format);
    PipelineBuilder &SetDepthFormat(VkFormat format);

    PipelineBuilder &EnableDepthTest();
    PipelineBuilder &EnableBlending();

    PipelineBuilder &AddDescriptorSetLayout(VkDescriptorSetLayout descriptor);

    PipelineBuilder &SetPushConstantSize(uint32_t size);

    Pipeline Build(VulkanContext &ctx);

  private:
    void UpdateVertexInput();

  private:
    std::vector<VkPipelineShaderStageCreateInfo> mShaderStages;

    VkPipelineVertexInputStateCreateInfo mVertexInput;
    VkPipelineInputAssemblyStateCreateInfo mInputAssembly;
    VkPipelineRasterizationStateCreateInfo mRaster;
    VkPipelineMultisampleStateCreateInfo mMultisample;
    VkPipelineColorBlendAttachmentState mColorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo mColorBlend;
    VkPipelineDepthStencilStateCreateInfo mDepthStencil;

    VkFormat mColorFormat;
    std::optional<VkFormat> mDepthFormat;

    std::vector<VkDescriptorSetLayout> mDescriptorLayouts;

    uint32_t mPushConstantSize = 0;

    VkVertexInputBindingDescription mBindingDescription;
    std::vector<VkVertexInputAttributeDescription> mAttributeDescriptions;
};

class ComputePipelineBuilder {
  public:
    ComputePipelineBuilder() = default;

    ComputePipelineBuilder &SetShaderStage(VkPipelineShaderStageCreateInfo stage);

    Pipeline Build(VulkanContext &ctx, VkDescriptorSetLayout &descriptor);

  private:
    VkPipelineShaderStageCreateInfo mShaderStage;
};