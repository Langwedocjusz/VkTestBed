#include "Pipeline.h"
#include "Pch.h"

#include "Shader.h"
#include "Vassert.h"
#include "VertexLayout.h"
#include "VkUtils.h"

#include "volk.h"

#include <variant>

// === Graphics Pipeline Implementation: =======================================

void GraphicsPipeline::Bind(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, BindPoint, Handle);
}

void GraphicsPipeline::BindDescriptorSet(VkCommandBuffer cmd, VkDescriptorSet set, uint32_t setIdx)
{
    vkCmdBindDescriptorSets(cmd, BindPoint, Layout, setIdx, 1, &set, 0, nullptr);
}

void GraphicsPipeline::BindDescriptorSets(VkCommandBuffer cmd, std::span<VkDescriptorSet> sets, uint32_t startIdx)
{
    vkCmdBindDescriptorSets(
        cmd, BindPoint, Layout, startIdx, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr
    );
}

// === Compute Pipeline Implementation: =======================================

void ComputePipeline::Bind(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, BindPoint, Handle);
}

void ComputePipeline::BindDescriptorSet(VkCommandBuffer cmd, VkDescriptorSet set, uint32_t setIdx)
{
    vkCmdBindDescriptorSets(cmd, BindPoint, Layout, setIdx, 1, &set, 0, nullptr);
}

void ComputePipeline::BindDescriptorSets(VkCommandBuffer cmd, std::span<VkDescriptorSet> sets, uint32_t startIdx)
{
    vkCmdBindDescriptorSets(
        cmd, BindPoint, Layout, startIdx, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr
    );
}

void ComputePipeline::Dispatch(VkCommandBuffer cmd, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
{
    uint32_t dispCountX = (sizeX + LocalSize[0] - 1) / LocalSize[0];
    uint32_t dispCountY = (sizeY + LocalSize[1] - 1) / LocalSize[1];
    uint32_t dispCountZ = (sizeZ + LocalSize[2] - 1) / LocalSize[2];

    vkCmdDispatch(cmd, dispCountX, dispCountY, dispCountZ);
}

// === Graphics Pipeline Builder Implementation: ====================================

PipelineBuilder::PipelineBuilder(std::string_view debugName)
    : mDynamicStates({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR})
    , mDebugName(debugName)
{
    // Initialize all vulkan structs:
    mVertexInput          = {};
    mInputAssembly        = {};
    mRaster               = {};
    mMultisample          = {};
    mColorBlendAttachment = {};
    mColorBlend           = {};
    mDepthStencil         = {};

    mVertexInput.sType   = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    mInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    mRaster.sType        = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    mMultisample.sType   = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    mColorBlend.sType    = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    mDepthStencil.sType  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    // Disable depth and stencil testing by default:
    mDepthStencil.depthTestEnable   = VK_FALSE;
    mDepthStencil.stencilTestEnable = VK_FALSE;

    // Disable blending by default:
    mColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    mColorBlendAttachment.blendEnable = VK_FALSE;
    mColorBlend.attachmentCount       = 1;
    mColorBlend.pAttachments          = &mColorBlendAttachment;

    // Things that are hardcoded for now:
    mInputAssembly.primitiveRestartEnable = VK_FALSE;

    mRaster.depthClampEnable        = VK_FALSE;
    mRaster.depthBiasEnable         = VK_FALSE;
    mRaster.rasterizerDiscardEnable = VK_FALSE;
    mRaster.lineWidth               = 1.0f;

    mMultisample.sampleShadingEnable  = VK_FALSE;
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

PipelineBuilder &PipelineBuilder::SetVertexInput(
    const Vertex::Layout &layout, uint32_t binding, VkVertexInputRate inputRate
)
{
    vassert(
        std::holds_alternative<Vertex::PushLayout>(layout), "Pipeline vertex input can only be used with push layouts."
    );

    auto &pushLayout = std::get<Vertex::PushLayout>(layout);

    mBindingDescription    = Vertex::GetBindingDescription(pushLayout, binding, inputRate);
    mAttributeDescriptions = Vertex::GetAttributeDescriptions(pushLayout);

    UpdateVertexInput();
    return *this;
}

void PipelineBuilder::UpdateVertexInput()
{
    mVertexInput.vertexBindingDescriptionCount = 1;
    mVertexInput.pVertexBindingDescriptions    = &mBindingDescription;

    mVertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(mAttributeDescriptions.size());
    mVertexInput.pVertexAttributeDescriptions    = mAttributeDescriptions.data();
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

PipelineBuilder &PipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    mRaster.cullMode  = cullMode;
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
    mDepthStencil.depthTestEnable  = VK_TRUE;
    mDepthStencil.depthCompareOp   = compareOp;
    mDepthStencil.depthWriteEnable = VK_TRUE;

    // Hardcoded for now:
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.minDepthBounds        = 0.0f;
    mDepthStencil.maxDepthBounds        = 1.0f;

    return *this;
}

PipelineBuilder &PipelineBuilder::EnableDepthTestNoWrite(VkCompareOp compareOp)
{
    mDepthStencil.depthTestEnable  = VK_TRUE;
    mDepthStencil.depthCompareOp   = compareOp;
    mDepthStencil.depthWriteEnable = VK_FALSE;

    // Hardcoded for now:
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.minDepthBounds        = 0.0f;
    mDepthStencil.maxDepthBounds        = 1.0f;

    return *this;
}

PipelineBuilder &PipelineBuilder::EnableStencilTest(VkStencilOpState front, VkStencilOpState back)
{
    mDepthStencil.stencilTestEnable = VK_TRUE;
    mDepthStencil.front             = front;
    mDepthStencil.back              = back;

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
    mColorBlendAttachment.blendEnable = VK_TRUE;

    mColorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    mColorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    mColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    mColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    mColorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    mColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    mColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    mColorBlend.logicOpEnable     = VK_FALSE;
    mColorBlend.logicOp           = VK_LOGIC_OP_COPY;
    mColorBlend.attachmentCount   = 1;
    mColorBlend.pAttachments      = &mColorBlendAttachment;
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

PipelineBuilder &PipelineBuilder::SetMultisampling(VkSampleCountFlagBits sampleCount)
{
    mMultisample.rasterizationSamples = sampleCount;

    return *this;
}

GraphicsPipeline PipelineBuilder::Build(VulkanContext &ctx)
{
    return BuildImpl(ctx);
}

GraphicsPipeline PipelineBuilder::Build(VulkanContext &ctx, DeletionQueue &queue)
{
    const auto res = BuildImpl(ctx);

    queue.push_back(res.Handle);
    queue.push_back(res.Layout);

    return res;
}

GraphicsPipeline PipelineBuilder::BuildImpl(VulkanContext &ctx)
{
    GraphicsPipeline pipeline{};

    // Build the shader stages:
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
    uint32_t                                     pushConstantSize = 0;

    if (mVertexPath)
    {
        auto info = Shader::GetStageInfo(ctx, *mVertexPath, VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages.push_back(info.PipelineCreation);

        if (auto size = info.Reflection.PushConstantsSize)
        {
            pushConstantSize = std::max(pushConstantSize, *size);
        }
    }

    if (mFragmentPath)
    {
        auto info = Shader::GetStageInfo(ctx, *mFragmentPath, VK_SHADER_STAGE_FRAGMENT_BIT);
        shaderStages.push_back(info.PipelineCreation);

        if (auto size = info.Reflection.PushConstantsSize)
        {
            pushConstantSize = std::max(pushConstantSize, *size);
        }
    }

    // Create pipeline layout:
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (!mDescriptorLayouts.empty())
    {
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(mDescriptorLayouts.size());
        pipelineLayoutInfo.pSetLayouts    = mDescriptorLayouts.data();
    }

    // Fill in Push constant information:
    VkPushConstantRange pcRange{
        // TODO: may expose more granular control over this:
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .offset     = 0,
        .size       = pushConstantSize,
    };

    if (pushConstantSize > 0)
    {
        pipelineLayoutInfo.pPushConstantRanges    = &pcRange;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

    {
        auto ret = vkCreatePipelineLayout(ctx.Device, &pipelineLayoutInfo, nullptr, &pipeline.Layout);

        vassert(ret == VK_SUCCESS, "Failed to create a pipeline layout!");
    }

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_PIPELINE_LAYOUT, pipeline.Layout, mDebugName);

    // Fill in dynamic state info:
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(ctx.Swapchain->Extent.width);
    viewport.height   = static_cast<float>(ctx.Swapchain->Extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset   = {0, 0};
    scissor.extent   = ctx.Swapchain->Extent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports    = &viewport;
    viewport_state.scissorCount  = 1;
    viewport_state.pScissors     = &scissor;

    std::vector<VkDynamicState> dynStatesVec(mDynamicStates.begin(), mDynamicStates.end());

    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynStatesVec.size());
    dynamicInfo.pDynamicStates    = dynStatesVec.data();

    // Pipeline creation
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages             = shaderStages.data();
    pipelineInfo.pVertexInputState   = &mVertexInput;
    pipelineInfo.pInputAssemblyState = &mInputAssembly;
    pipelineInfo.pViewportState      = &viewport_state;
    pipelineInfo.pRasterizationState = &mRaster;
    pipelineInfo.pMultisampleState   = &mMultisample;
    pipelineInfo.pColorBlendState    = &mColorBlend;
    pipelineInfo.pDepthStencilState  = &mDepthStencil;
    pipelineInfo.pDynamicState       = &dynamicInfo;
    pipelineInfo.layout              = pipeline.Layout;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

    // Vulkan 1.3 Dynamic Rendering:
    // New info struct:
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;

    if (mColorFormat.has_value())
    {
        pipelineRenderingCreateInfo.colorAttachmentCount    = 1;
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
        auto ret = vkCreateGraphicsPipelines(ctx.Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.Handle);

        vassert(ret == VK_SUCCESS, "Failed to create a Graphics Pipeline!");
    }

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_PIPELINE, pipeline.Handle, mDebugName);

    for (auto &shaderInfo : shaderStages)
        vkDestroyShaderModule(ctx.Device, shaderInfo.module, nullptr);

    return pipeline;
}

// === Compute Pipeline Builder Implementation: ====================================

ComputePipelineBuilder::ComputePipelineBuilder(std::string_view debugName)
    : mDebugName(debugName)
{
}

ComputePipelineBuilder &ComputePipelineBuilder::SetShaderPath(std::string_view path)
{
    mShaderPath = std::string(path);
    return *this;
}

ComputePipelineBuilder &ComputePipelineBuilder::AddDescriptorSetLayout(VkDescriptorSetLayout descriptor)
{
    mDescriptorLayouts.push_back(descriptor);

    return *this;
}

ComputePipeline ComputePipelineBuilder::Build(VulkanContext &ctx)
{
    return BuildImpl(ctx);
}

ComputePipeline ComputePipelineBuilder::Build(VulkanContext &ctx, DeletionQueue &queue)
{
    const auto res = BuildImpl(ctx);

    queue.push_back(res.Handle);
    queue.push_back(res.Layout);

    return res;
}

ComputePipeline ComputePipelineBuilder::BuildImpl(VulkanContext &ctx)
{
    auto info = Shader::GetStageInfo(ctx, *mShaderPath, VK_SHADER_STAGE_COMPUTE_BIT);

    ComputePipeline pipeline{};
    pipeline.LocalSize = *info.Reflection.LocalSize;

    uint32_t pcSize = 0;
    if (auto size = info.Reflection.PushConstantsSize)
    {
        pcSize = *size;
    }

    std::array shaderStages{info.PipelineCreation};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (!mDescriptorLayouts.empty())
    {
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(mDescriptorLayouts.size());
        pipelineLayoutInfo.pSetLayouts    = mDescriptorLayouts.data();
    }

    // Fill in push constant info:
    VkPushConstantRange pcRange{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset     = 0,
        .size       = pcSize,
    };

    if (pcSize > 0)
    {
        pipelineLayoutInfo.pPushConstantRanges    = &pcRange;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

    {
        auto ret = vkCreatePipelineLayout(ctx.Device, &pipelineLayoutInfo, nullptr, &pipeline.Layout);

        vassert(ret == VK_SUCCESS, "Failed to create compute pipeline layout!");
    }

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_PIPELINE_LAYOUT, pipeline.Layout, mDebugName);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipeline.Layout;
    pipelineInfo.stage  = shaderStages[0];

    {
        auto ret = vkCreateComputePipelines(ctx.Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.Handle);

        vassert(ret == VK_SUCCESS, "Failed to create compute pipeline!");
    }

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_PIPELINE, pipeline.Handle, mDebugName);

    vkDestroyShaderModule(ctx.Device, shaderStages[0].module, nullptr);

    return pipeline;
}