#pragma once

#include "Vertex.h"
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

    PipelineBuilder SetShaderStages(std::vector<VkPipelineShaderStageCreateInfo> stages)
    {
        mShaderStages = stages;
        return *this;
    }

    /// If vertex input is not set, vertex data can't be accessed the usual way
    /// in vertex shaders. This is actually the desired behaviour when doing
    /// vertex pulling or generating vertices on-the-fly in the shader itself.
    template <Vertex V>
    PipelineBuilder SetVertexInput(uint32_t binding, VkVertexInputRate inputRate)
    {
        mBindingDescription = GetBindingDescription<V>(binding, inputRate);
        mAttributeDescriptions = V::GetAttributeDescriptions();

        UpdateVertexInput();
        return *this;
    }

    PipelineBuilder SetTopology(VkPrimitiveTopology topo);
    PipelineBuilder SetPolygonMode(VkPolygonMode mode);
    PipelineBuilder SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);

    PipelineBuilder SetColorFormat(VkFormat format);
    PipelineBuilder SetDepthFormat(VkFormat format);

    PipelineBuilder EnableDepthTest();
    PipelineBuilder EnableBlending();

    PipelineBuilder SetDescriptorSetLayout(VkDescriptorSetLayout &descriptor);

    PipelineBuilder SetPushConstantSize(uint32_t size);

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

    uint32_t mLayoutCount = 0;
    VkDescriptorSetLayout *mLayoutsPtr = nullptr;

    uint32_t mPushConstantSize = 0;

    VkVertexInputBindingDescription mBindingDescription;
    std::vector<VkVertexInputAttributeDescription> mAttributeDescriptions;
};

class ComputePipelineBuilder {
  public:
    ComputePipelineBuilder() = default;

    ComputePipelineBuilder SetShaderStage(VkPipelineShaderStageCreateInfo stage)
    {
        mShaderStage = stage;
        return *this;
    }

    Pipeline Build(VulkanContext &ctx, VkDescriptorSetLayout &descriptor);

  private:
    VkPipelineShaderStageCreateInfo mShaderStage;
};