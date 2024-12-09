#include "Pipeline.h"
#include <vulkan/vulkan_core.h>

PipelineBuilder::PipelineBuilder()
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

    // Disable depht testing by default:
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

PipelineBuilder &PipelineBuilder::SetShaderStages(
    const std::vector<VkPipelineShaderStageCreateInfo> &stages)
{
    mShaderStages = stages;
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

PipelineBuilder &PipelineBuilder::EnableDepthTest()
{
    mDepthStencil.depthTestEnable = VK_TRUE;
    mDepthStencil.depthWriteEnable = VK_TRUE;
    mDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.minDepthBounds = 0.0f; // Optional
    mDepthStencil.maxDepthBounds = 1.0f; // Optional
    mDepthStencil.stencilTestEnable = VK_FALSE;
    mDepthStencil.front = {}; // Optional
    mDepthStencil.back = {};  // Optional
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
    mPushConstantSize = size;

    return *this;
}

Pipeline PipelineBuilder::Build(VulkanContext &ctx)
{
    Pipeline pipeline;

    // Create pipeline layout:
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (!mDescriptorLayouts.empty())
    {
        pipelineLayoutInfo.setLayoutCount =
            static_cast<uint32_t>(mDescriptorLayouts.size());
        pipelineLayoutInfo.pSetLayouts = mDescriptorLayouts.data();
    }

    if (mPushConstantSize != 0)
    {
        VkPushConstantRange pushConstant{};
        pushConstant.offset = 0;
        pushConstant.size = mPushConstantSize;
        // To-do: may expose more granular control over this:
        pushConstant.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;

        pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

    if (vkCreatePipelineLayout(ctx.Device, &pipelineLayoutInfo, nullptr,
                               &pipeline.Layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create a pipeline layout!");

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

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicInfo = {};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicInfo.pDynamicStates = dynamicStates.data();

    // Pipeline creation
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(mShaderStages.size());
    pipelineInfo.pStages = mShaderStages.data();
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
    VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &mColorFormat;

    if (mDepthFormat.has_value())
    {
        pipelineRenderingCreateInfo.depthAttachmentFormat = mDepthFormat.value();
        // pipelineRenderingCreateInfo.stencilAttachmentFormat = mDepthStencilFormat;
    }

    //  Chain into the pipeline create info
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;

    if (vkCreateGraphicsPipelines(ctx.Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &pipeline.Handle) != VK_SUCCESS)
        throw std::runtime_error("Failed to create a Graphics Pipeline!");

    for (auto &shaderInfo : mShaderStages)
        vkDestroyShaderModule(ctx.Device, shaderInfo.module, nullptr);

    return pipeline;
}

ComputePipelineBuilder &ComputePipelineBuilder::SetShaderStage(
    VkPipelineShaderStageCreateInfo stage)
{
    mShaderStage = stage;
    return *this;
}

Pipeline ComputePipelineBuilder::Build(VulkanContext &ctx,
                                       VkDescriptorSetLayout &descriptor)
{
    Pipeline pipeline;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptor;

    if (vkCreatePipelineLayout(ctx.Device, &pipelineLayoutInfo, nullptr,
                               &pipeline.Layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create compute pipeline layout!");

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipeline.Layout;
    pipelineInfo.stage = mShaderStage;

    if (vkCreateComputePipelines(ctx.Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                 &pipeline.Handle) != VK_SUCCESS)
        throw std::runtime_error("Failed to create compute pipeline!");

    vkDestroyShaderModule(ctx.Device, mShaderStage.module, nullptr);

    return pipeline;
}