#include "HelloRenderer.h"
#include "Descriptor.h"
#include "GeometryProvider.h"
#include "Renderer.h"
#include "Shader.h"
#include "Utils.h"
#include "Vertex.h"
#include "VkInit.h"

#include "Common.h"

#include <cstdint>
#include <iostream>
#include <ranges>
#include <vulkan/vulkan.h>

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info,
                             RenderContext::Queues &queues)
    : IRenderer(ctx, info, queues)
{
    // Create descriptor sets:
    //  Descriptor layout
    mDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .Build(ctx);

    // Descriptor pool
    uint32_t maxSets = mFrame.MaxInFlight;

    std::vector<Descriptor::PoolCount> poolCounts{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets},
    };

    mDescriptorPool = Descriptor::InitPool(ctx, maxSets, poolCounts);

    // Descriptor sets allocation
    std::vector<VkDescriptorSetLayout> layouts(mFrame.MaxInFlight, mDescriptorSetLayout);

    mDescriptorSets = Descriptor::Allocate(ctx, mDescriptorPool, layouts);

    mMainDeletionQueue.push_back(mDescriptorPool);
    mMainDeletionQueue.push_back(mDescriptorSetLayout);

    // Create Graphics Pipelines
    auto shaderStages = ShaderBuilder()
                            .SetVertexPath("assets/spirv/HelloTriangleVert.spv")
                            .SetFragmentPath("assets/spirv/HelloTriangleFrag.spv")
                            .Build(ctx);

    mGraphicsPipeline = PipelineBuilder()
                            .SetShaderStages(shaderStages)
                            .SetVertexInput<ColoredVertex>(0, VK_VERTEX_INPUT_RATE_VERTEX)
                            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                            .SetPolygonMode(VK_POLYGON_MODE_FILL)
                            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
                            .SetColorFormat(mRenderTargetFormat)
                            .SetPushConstantSize(sizeof(glm::mat4))
                            .SetDescriptorSetLayout(mDescriptorSetLayout)
                            .Build(ctx);

    mMainDeletionQueue.push_back(mGraphicsPipeline.Handle);
    mMainDeletionQueue.push_back(mGraphicsPipeline.Layout);

    // Create Uniform Buffers:
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    mUniformBuffers.resize(mFrame.MaxInFlight);

    for (auto &uniformBuffer : mUniformBuffers)
    {
        uniformBuffer = Buffer::CreateMappedUniformBuffer(ctx, bufferSize);
        mMainDeletionQueue.push_back(&uniformBuffer);
    }

    // Update descriptor sets:
    for (size_t i = 0; i < mDescriptorSets.size(); i++)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = mUniformBuffers[i].Handle;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = mDescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(ctx.Device, 1, &descriptorWrite, 0, nullptr);
    }

    // Create swapchain resources:
    CreateSwapchainResources();
}

HelloRenderer::~HelloRenderer()
{
    mSwapchainDeletionQueue.flush();
    mMainDeletionQueue.flush();
    mSceneDeletionQueue.flush();
}

void HelloRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
    // Basic orthographic camera setup:
    auto width = static_cast<float>(mCtx.Swapchain.extent.width);
    auto height = static_cast<float>(mCtx.Swapchain.extent.height);

    float sx = 1.0f, sy = 1.0f;

    if (height < width)
        sx = width / height;
    else
        sy = height / width;

    auto proj = glm::ortho(-sx, sx, -sy, sy, -1.0f, 1.0f);

    mUBOData.ViewProjection = proj;

    auto &uniformBuffer = mUniformBuffers[mFrame.Index];
    Buffer::UploadToMappedBuffer(uniformBuffer, &mUBOData, sizeof(mUBOData));
}

void HelloRenderer::OnImGui()
{
}

void HelloRenderer::OnRender()
{
    auto &cmd = mFrame.CurrentCmd();

    VkClearValue clear{{{0.0f, 0.0f, 0.0f, 0.0f}}};

    VkRenderingAttachmentInfoKHR colorAttachment = vkinit::CreateAttachmentInfo(
        mRenderTargetView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR, clear);

    VkRenderingInfoKHR renderingInfo =
        vkinit::CreateRenderingInfo(GetTargetSize(), colorAttachment);

    vkCmdBeginRendering(cmd, &renderingInfo);
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline.Handle);

        common::ViewportScissor(cmd, GetTargetSize());

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mGraphicsPipeline.Layout, 0, 1,
                                &mDescriptorSets[mFrame.Index], 0, nullptr);

        for (auto &drawable : mDrawables)
        {
            std::array<VkBuffer, 1> vertexBuffers{drawable.VertexBuffer.Handle};
            std::array<VkDeviceSize, 1> offsets{0};

            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers.data(), offsets.data());

            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0,
                                 VK_INDEX_TYPE_UINT16);

            for (auto &transform : drawable.Transforms)
            {
                vkCmdPushConstants(cmd, mGraphicsPipeline.Layout,
                                   VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(transform),
                                   &transform);
                vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
            }
        }
    }
    vkCmdEndRendering(cmd);
}

void HelloRenderer::CreateSwapchainResources()
{
    // Create the render target:
    auto ScaleResolution = [this](uint32_t res) {
        return static_cast<uint32_t>(mInternalResolutionScale * static_cast<float>(res));
    };

    uint32_t width = ScaleResolution(mCtx.Swapchain.extent.width);
    uint32_t height = ScaleResolution(mCtx.Swapchain.extent.height);

    VkExtent3D drawExtent{
        .width = width,
        .height = height,
        .depth = 1,
    };

    VkImageUsageFlags drawUsage{};
    drawUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    ImageInfo renderTargetInfo{
        .Extent = drawExtent,
        .Format = mRenderTargetFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = drawUsage,
    };

    mRenderTarget = Image::CreateImage2D(mCtx, renderTargetInfo);
    mSwapchainDeletionQueue.push_back(&mRenderTarget);

    // Create the render target view:
    mRenderTargetView = Image::CreateView2D(mCtx, mRenderTarget, mRenderTargetFormat,
                                            VK_IMAGE_ASPECT_COLOR_BIT);
    mSwapchainDeletionQueue.push_back(mRenderTargetView);
}

void HelloRenderer::LoadScene(Scene &scene)
{
    if (scene.GlobalUpdate)
    {
        // This will only take efect on non-first runs:
        mSceneDeletionQueue.flush();
        mDrawables.clear();

        LoadProviders(scene);
    }

    LoadInstances(scene);
}

void HelloRenderer::LoadProviders(Scene &scene)
{
    for (auto &obj : scene.Objects)
    {
        auto vertOpt = obj.Provider.UnpackVertices<ColoredVertex>();
        auto idxOpt = obj.Provider.UnpackIndices<uint16_t>();

        if (!vertOpt.has_value())
        {
            std::cerr << "Unsupported vertex type.\n";
            continue;
        }

        if (!idxOpt.has_value())
        {
            std::cerr << "Unsupported index type.\n";
            continue;
        }

        auto vertFn = vertOpt.value();
        auto idxFn = idxOpt.value();

        mDrawables.emplace_back();
        auto &drawable = mDrawables.back();

        auto &pool = mFrame.CurrentPool();

        // Create Vertex buffer:
        {
            utils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

            auto vertices = vertFn();
            auto vertexCount = vertices.size();

            GPUBufferInfo vertInfo{
                .Usage =
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .Size = vertexCount * sizeof(ColoredVertex),
                .Data = vertices.data(),
            };

            drawable.VertexBuffer = Buffer::CreateGPUBuffer(mCtx, cmd.Buffer, vertInfo);
            drawable.VertexCount = vertexCount;
        }

        // Create Index buffer:
        {
            utils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

            auto indices = idxFn();
            auto indexCount = indices.size();

            GPUBufferInfo idxInfo{
                .Usage =
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .Size = indexCount * sizeof(uint16_t),
                .Data = indices.data(),
            };

            drawable.IndexBuffer = Buffer::CreateGPUBuffer(mCtx, cmd.Buffer, idxInfo);
            drawable.IndexCount = indexCount;
        }
    }

    for (auto &drawable : mDrawables)
    {
        mSceneDeletionQueue.push_back(&drawable.VertexBuffer);
        mSceneDeletionQueue.push_back(&drawable.IndexBuffer);
    }
}

void HelloRenderer::LoadInstances(Scene &scene)
{
    using namespace std::views;

    size_t drawableId = 0;

    for (auto& obj : scene.Objects)
    {
        auto vertOpt = obj.Provider.UnpackVertices<ColoredVertex>();
        auto idxOpt = obj.Provider.UnpackIndices<uint16_t>();

        if (!vertOpt.has_value())
        {
            std::cerr << "Unsupported vertex type.\n";
            continue;
        }

        if (!idxOpt.has_value())
        {
            std::cerr << "Unsupported index type.\n";
            continue;
        }

        auto& drawable = mDrawables[drawableId];

        if (obj.UpdateInstances)
        {
            drawable.Transforms.clear();

            // Unpack instance data:
            for (auto instance : obj.Instances)
            {
                glm::mat4 transform(1.0f);

                transform = glm::scale(transform, instance.Scale);

                glm::mat4 rotation = glm::mat4_cast(instance.Rotation);
                transform = rotation * transform;

                glm::mat4 translation =
                    glm::translate(glm::mat4(1.0f), instance.Translation);
                transform = translation * transform;

                drawable.Transforms.push_back(transform);
            }
        }

        drawableId++;
    }
}
