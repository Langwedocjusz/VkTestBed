#include "Pipeline.h"
#include "Pch.h"

#include "Shader.h"
#include "Vassert.h"
#include "VkUtils.h"

#include <vulkan/vulkan.h>

Pipeline Pipeline::MakePipeline(VkPipelineBindPoint bindPoint)
{
    Pipeline ret;
    ret.mBindPoint = bindPoint;

    return ret;
}

void Pipeline::Bind(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, mBindPoint, Handle);
}

void Pipeline::BindDescriptorSet(VkCommandBuffer cmd, VkDescriptorSet set,
                                         uint32_t setIdx)
{
    vkCmdBindDescriptorSets(cmd, mBindPoint, Layout, setIdx, 1, &set,
                            0, nullptr);
}

void Pipeline::BindDescriptorSets(VkCommandBuffer cmd,
                                          std::span<VkDescriptorSet> sets,
                                          uint32_t startIdx)
{
    vkCmdBindDescriptorSets(cmd, mBindPoint, Layout, startIdx,
                            static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
}

PipelineBuilder::PipelineBuilder(std::string_view debugName)
    : mDynamicStates({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}),
      mDebugName(debugName)
{
    // Initialize all vulkan structs:
    mVertexInput = {};
    mInputAssembly = {};
    mRaster = {};
    mMultisample = {};
    mColorBlendAttachment = {};
    mColorBlend = {};
    mDepthStencil = {};

    mVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    mInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    mRaster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    mMultisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    mColorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    mDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    // Disable depth and stencil testing by default:
    mDepthStencil.depthTestEnable = VK_FALSE;
    mDepthStencil.stencilTestEnable = VK_FALSE;

    // Disable blending by default:
    mColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    mColorBlendAttachment.blendEnable = VK_FALSE;
    mColorBlend.attachmentCount = 1;
    mColorBlend.pAttachments = &mColorBlendAttachment;

    // Things that are hardcoded for now:
    mInputAssembly.primitiveRestartEnable = VK_FALSE;

    mRaster.depthClampEnable = VK_FALSE;
    mRaster.depthBiasEnable = VK_FALSE;
    mRaster.rasterizerDiscardEnable = VK_FALSE;
    mRaster.lineWidth = 1.0f;

    mMultisample.sampleShadingEnable = VK_FALSE;
    mMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
}

PipelineBuilder &PipelineBuilder::SetShaderPathVertex(std::string_view path)
{
    mVertexPath = std::string(path);
    return *this;
}

PipelineBuilder &PipelineBuilder::SetShaderPathFragment(std::string_view path)
{
    mFragmentPath = std::string(path);
    return *this;
}

PipelineBuilder &PipelineBuilder::SetVertexInput(const Vertex::Layout &layout,
                                                 uint32_t binding,
                                                 VkVertexInputRate inputRate)
{
    mBindingDescription = Vertex::GetBindingDescription(layout, binding, inputRate);
    mAttributeDescriptions = Vertex::GetAttributeDescriptions(layout);

    UpdateVertexInput();
    return *this;
}

void PipelineBuilder::UpdateVertexInput()
{
    mVertexInput.vertexBindingDescriptionCount = 1;
    mVertexInput.pVertexBindingDescriptions = &mBindingDescription;

    mVertexInput.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(mAttributeDescriptions.size());
    mVertexInput.pVertexAttributeDescriptions = mAttributeDescriptions.data();
}

PipelineBuilder &PipelineBuilder::SetTopology(VkPrimitiveTopology topo)
{
    mInputAssembly.topology = topo;
    return *this;
}

PipelineBuilder &PipelineBuilder::SetPolygonMode(VkPolygonMode mode)
{
    mRaster.polygonMode = mode;
    return *this;
}

PipelineBuilder &PipelineBuilder::SetCullMode(VkCullModeFlags cullMode,
                                              VkFrontFace frontFace)
{
    mRaster.cullMode = cullMode;
    mRaster.frontFace = frontFace;
    return *this;
}

PipelineBuilder &PipelineBuilder::RequestDynamicState(VkDynamicState state)
{
    mDynamicStates.insert(state);
    return *this;
}

PipelineBuilder &PipelineBuilder::EnableDepthTest(VkCompareOp compareOp)
{
    mDepthStencil.depthTestEnable = VK_TRUE;
    mDepthStencil.depthCompareOp = compareOp;
    mDepthStencil.depthWriteEnable = VK_TRUE;

    // Hardcoded for now:
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.minDepthBounds = 0.0f;
    mDepthStencil.maxDepthBounds = 1.0f;

    return *this;
}

PipelineBuilder &PipelineBuilder::EnableDepthTestNoWrite(VkCompareOp compareOp)
{
    mDepthStencil.depthTestEnable = VK_TRUE;
    mDepthStencil.depthCompareOp = compareOp;
    mDepthStencil.depthWriteEnable = VK_FALSE;

    // Hardcoded for now:
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.minDepthBounds = 0.0f;
    mDepthStencil.maxDepthBounds = 1.0f;

    return *this;
}

PipelineBuilder &PipelineBuilder::EnableStencilTest(VkStencilOpState front,
                                                    VkStencilOpState back)
{
    mDepthStencil.stencilTestEnable = VK_TRUE;
    mDepthStencil.front = front;
    mDepthStencil.back = back;

    return *this;
}

PipelineBuilder &PipelineBuilder::SetColorFormat(VkFormat format)
{
    mColorFormat = format;
    return *this;
}

PipelineBuilder &PipelineBuilder::SetDepthFormat(VkFormat format)
{
    mDepthFormat = format;
    return *this;
}

PipelineBuilder &PipelineBuilder::SetStencilFormat(VkFormat format)
{
    mStencilFormat = format;
    return *this;
}

PipelineBuilder &PipelineBuilder::EnableBlending()
{
    mColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    mColorBlendAttachment.blendEnable = VK_TRUE;
    mColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    mColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    mColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    mColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    mColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    mColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    mColorBlend.logicOpEnable = VK_FALSE;
    mColorBlend.logicOp = VK_LOGIC_OP_COPY;
    mColorBlend.attachmentCount = 1;
    mColorBlend.pAttachments = &mColorBlendAttachment;
    mColorBlend.blendConstants[0] = 0.0f;
    mColorBlend.blendConstants[1] = 0.0f;
    mColorBlend.blendConstants[2] = 0.0f;
    mColorBlend.blendConstants[3] = 0.0f;

    return *this;
}

PipelineBuilder &PipelineBuilder::AddDescriptorSetLayout(VkDescriptorSetLayout descriptor)
{
    mDescriptorLayouts.push_back(descriptor);

    return *this;
}

PipelineBuilder &PipelineBuilder::SetPushConstantSize(uint32_t size)
{
    VkPushConstantRange pcRange{};
    pcRange.offset = 0;
    pcRange.size = size;
    // To-do: may expose more granular control over this:
    pcRange.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;

    mPushConstantRange = pcRange;

    return *this;
}

Pipeline PipelineBuilder::Build(VulkanContext &ctx)
{
    return BuildImpl(ctx);
}

Pipeline PipelineBuilder::Build(VulkanContext &ctx, DeletionQueue &queue)
{
    const auto res = BuildImpl(ctx);

    queue.push_back(res.Handle);
    queue.push_back(res.Layout);

    return res;
}

Pipeline PipelineBuilder::BuildImpl(VulkanContext &ctx)
{
    auto pipeline = Pipeline::MakePipeline(VK_PIPELINE_BIND_POINT_GRAPHICS);

    // Build the shader stages:
    auto shaderStages = ShaderBuilder()
                            .SetVertexPath(*mVertexPath)
                            .SetFragmentPath(*mFragmentPath)
                            .Build(ctx);

    // Create pipeline layout:
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (!mDescriptorLayouts.empty())
    {
        pipelineLayoutInfo.setLayoutCount =
            static_cast<uint32_t>(mDescriptorLayouts.size());
        pipelineLayoutInfo.pSetLayouts = mDescriptorLayouts.data();
    }

    if (mPushConstantRange)
    {
        auto &pcRange = *mPushConstantRange;

        pipelineLayoutInfo.pPushConstantRanges = &pcRange;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

    {
        auto ret = vkCreatePipelineLayout(ctx.Device, &pipelineLayoutInfo, nullptr,
                                          &pipeline.Layout);

        vassert(ret == VK_SUCCESS, "Failed to create a pipeline layout!");
    }

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_PIPELINE_LAYOUT, pipeline.Layout,
                          mDebugName);

    // Fill in dynamic state info:
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(ctx.Swapchain.extent.width);
    viewport.height = static_cast<float>(ctx.Swapchain.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = ctx.Swapchain.extent;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    std::vector<VkDynamicState> dynStatesVec(mDynamicStates.begin(),
                                             mDynamicStates.end());

    VkPipelineDynamicStateCreateInfo dynamicInfo = {};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynStatesVec.size());
    dynamicInfo.pDynamicStates = dynStatesVec.data();

    // Pipeline creation
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &mVertexInput;
    pipelineInfo.pInputAssemblyState = &mInputAssembly;
    pipelineInfo.pViewportState = &viewport_state;
    pipelineInfo.pRasterizationState = &mRaster;
    pipelineInfo.pMultisampleState = &mMultisample;
    pipelineInfo.pColorBlendState = &mColorBlend;
    pipelineInfo.pDepthStencilState = &mDepthStencil;
    pipelineInfo.pDynamicState = &dynamicInfo;
    pipelineInfo.layout = pipeline.Layout;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    // Vulkan 1.3 Dynamic Rendering:
    // New info struct:
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;

    if (mColorFormat.has_value())
    {
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        pipelineRenderingCreateInfo.pColorAttachmentFormats = &mColorFormat.value();
    }

    if (mDepthFormat.has_value())
    {
        pipelineRenderingCreateInfo.depthAttachmentFormat = mDepthFormat.value();
    }

    if (mStencilFormat.has_value())
    {
        pipelineRenderingCreateInfo.stencilAttachmentFormat = mStencilFormat.value();
    }

    if (mDepthFormat.has_value() && mStencilFormat.has_value())
    {
        vassert(*mDepthFormat == *mStencilFormat);
    }

    //  Chain into the pipeline create info
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;

    {
        auto ret = vkCreateGraphicsPipelines(ctx.Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                             nullptr, &pipeline.Handle);

        vassert(ret == VK_SUCCESS, "Failed to create a Graphics Pipeline!");
    }

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_PIPELINE, pipeline.Handle, mDebugName);

    for (auto &shaderInfo : shaderStages)
        vkDestroyShaderModule(ctx.Device, shaderInfo.module, nullptr);

    return pipeline;
}

ComputePipelineBuilder::ComputePipelineBuilder(std::string_view debugName)
    : mDebugName(debugName)
{
}

ComputePipelineBuilder &ComputePipelineBuilder::SetShaderPath(std::string_view path)
{
    mShaderPath = std::string(path);
    return *this;
}

ComputePipelineBuilder &ComputePipelineBuilder::AddDescriptorSetLayout(
    VkDescriptorSetLayout descriptor)
{
    mDescriptorLayouts.push_back(descriptor);

    return *this;
}

ComputePipelineBuilder &ComputePipelineBuilder::SetPushConstantSize(uint32_t size)
{
    VkPushConstantRange pcRange{};
    pcRange.offset = 0;
    pcRange.size = size;
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    mPushConstantRange = pcRange;
    return *this;
}

Pipeline ComputePipelineBuilder::Build(VulkanContext &ctx)
{
    return BuildImpl(ctx);
}

Pipeline ComputePipelineBuilder::Build(VulkanContext &ctx, DeletionQueue &queue)
{
    const auto res = BuildImpl(ctx);

    queue.push_back(res.Handle);
    queue.push_back(res.Layout);

    return res;
}

Pipeline ComputePipelineBuilder::BuildImpl(VulkanContext &ctx)
{
    auto pipeline = Pipeline::MakePipeline(VK_PIPELINE_BIND_POINT_COMPUTE);

    auto shaderStages = ShaderBuilder().SetComputePath(*mShaderPath).Build(ctx);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (!mDescriptorLayouts.empty())
    {
        pipelineLayoutInfo.setLayoutCount =
            static_cast<uint32_t>(mDescriptorLayouts.size());
        pipelineLayoutInfo.pSetLayouts = mDescriptorLayouts.data();
    }

    if (mPushConstantRange)
    {
        auto &pcRange = *mPushConstantRange;

        pipelineLayoutInfo.pPushConstantRanges = &pcRange;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

    {
        auto ret = vkCreatePipelineLayout(ctx.Device, &pipelineLayoutInfo, nullptr,
                                          &pipeline.Layout);

        vassert(ret == VK_SUCCESS, "Failed to create compute pipeline layout!");
    }

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_PIPELINE_LAYOUT, pipeline.Layout,
                          mDebugName);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipeline.Layout;
    pipelineInfo.stage = shaderStages[0];

    {
        auto ret = vkCreateComputePipelines(ctx.Device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                            nullptr, &pipeline.Handle);

        vassert(ret == VK_SUCCESS, "Failed to create compute pipeline!");
    }

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_PIPELINE, pipeline.Handle, mDebugName);

    vkDestroyShaderModule(ctx.Device, shaderStages[0].module, nullptr);

    return pipeline;
}