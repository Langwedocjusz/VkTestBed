#include "Minimal3D.h"
#include "Barrier.h"
#include "Common.h"
#include "Descriptor.h"
#include "GeometryProvider.h"
#include "ImageLoaders.h"
#include "MeshBuffers.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Shader.h"
#include "VkInit.h"

#include <cstdint>
#include <format>
#include <iostream>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

Minimal3DRenderer::Minimal3DRenderer(VulkanContext &ctx, FrameInfo &info,
                                     RenderContext::Queues &queues,
                                     std::unique_ptr<Camera> &camera)
    : IRenderer(ctx, info, queues, camera)
{
    // Create descriptor set layout for sampling textures
    mTextureDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx);

    // Create a single descriptor pool (temporary, will crash if
    // if it runs out of space)
    //  Descriptor pool
    const uint32_t maxSets = 32;

    std::vector<Descriptor::PoolCount> poolCounts{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets},
    };

    mTextureDescriptorPool = Descriptor::InitPool(ctx, maxSets, poolCounts);

    mMainDeletionQueue.push_back(mTextureDescriptorPool);
    mMainDeletionQueue.push_back(mTextureDescriptorSetLayout);

    // Create Graphics Pipelines
    auto coloredShaderStages =
        ShaderBuilder()
            .SetVertexPath("assets/spirv/Minimal3DColoredVert.spv")
            .SetFragmentPath("assets/spirv/Minimal3DColoredFrag.spv")
            .Build(ctx);

    mColoredPipeline =
        PipelineBuilder()
            .SetShaderStages(coloredShaderStages)
            .SetVertexInput(mColoredLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .SetPushConstantSize(sizeof(glm::mat4))
            .AddDescriptorSetLayout(mCamera->DescriptorSetLayout())
            .EnableDepthTest()
            .SetDepthFormat(mDepthFormat)
            .Build(ctx);

    mMainDeletionQueue.push_back(mColoredPipeline.Handle);
    mMainDeletionQueue.push_back(mColoredPipeline.Layout);

    auto textuedShaderStages =
        ShaderBuilder()
            .SetVertexPath("assets/spirv/Minimal3DTexturedVert.spv")
            .SetFragmentPath("assets/spirv/Minimal3DTexturedFrag.spv")
            .Build(ctx);

    mTexturedPipeline =
        PipelineBuilder()
            .SetShaderStages(textuedShaderStages)
            .SetVertexInput(mTexturedLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .SetPushConstantSize(sizeof(glm::mat4))
            .AddDescriptorSetLayout(mCamera->DescriptorSetLayout())
            .AddDescriptorSetLayout(mTextureDescriptorSetLayout)
            .EnableDepthTest()
            .SetDepthFormat(mDepthFormat)
            .Build(ctx);

    mMainDeletionQueue.push_back(mTexturedPipeline.Handle);
    mMainDeletionQueue.push_back(mTexturedPipeline.Layout);

    // Create the texture sampler:
    mSampler = SamplerBuilder()
                   .SetMagFilter(VK_FILTER_LINEAR)
                   .SetMinFilter(VK_FILTER_LINEAR)
                   .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                   .Build(mCtx);

    mMainDeletionQueue.push_back(mSampler);

    // Create swapchain resources:
    CreateSwapchainResources();
}

Minimal3DRenderer::~Minimal3DRenderer()
{
    mSwapchainDeletionQueue.flush();
    mSceneDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void Minimal3DRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
}

void Minimal3DRenderer::OnImGui()
{
}

void Minimal3DRenderer::OnRender()
{
    auto &cmd = mFrame.CurrentCmd();

    barrier::ImageBarrierDepthToRender(cmd, mDepthBuffer.Handle);

    VkClearValue clear;
    clear.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

    auto colorAttachment = vkinit::CreateAttachmentInfo(
        mRenderTargetView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, clear);

    VkClearValue depthClear;
    depthClear.depthStencil = {1.0f, 0};

    auto depthAttachment = vkinit::CreateAttachmentInfo(
        mDepthBufferView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, depthClear);

    VkRenderingInfoKHR renderingInfo =
        vkinit::CreateRenderingInfo(GetTargetSize(), colorAttachment, depthAttachment);

    vkCmdBeginRendering(cmd, &renderingInfo);
    {
        // 1. Colored vertices pass:
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mColoredPipeline.Handle);

        common::ViewportScissor(cmd, GetTargetSize());

        // To-do: figure out a better way of doing this:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mColoredPipeline.Layout, 0, 1, mCamera->DescriptorSet(),
                                0, nullptr);

        for (auto &drawable : mColoredDrawables)
        {
            std::array<VkBuffer, 1> vertexBuffers{drawable.VertexBuffer.Handle};
            std::array<VkDeviceSize, 1> offsets{0};

            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers.data(), offsets.data());
            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0,
                                 mColoredLayout.IndexType);

            for (auto &transform : drawable.Transforms)
            {
                vkCmdPushConstants(cmd, mColoredPipeline.Layout,
                                   VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(transform),
                                   &transform);
                vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
            }
        }

        // 2. Textured vertices pass:
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mTexturedPipeline.Handle);

        common::ViewportScissor(cmd, GetTargetSize());

        // To-do: figure out a better way of doing this:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mTexturedPipeline.Layout, 0, 1, mCamera->DescriptorSet(),
                                0, nullptr);

        for (auto &drawable : mTexturedDrawables)
        {
            std::array<VkBuffer, 1> vertexBuffers{drawable.VertexBuffer.Handle};
            std::array<VkDeviceSize, 1> offsets{0};

            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers.data(), offsets.data());
            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0,
                                 mTexturedLayout.IndexType);

            auto &texture = mTextures[drawable.TextureId];

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    mTexturedPipeline.Layout, 1, 1,
                                    &texture.DescriptorSet, 0, nullptr);

            for (auto &transform : drawable.Transforms)
            {
                vkCmdPushConstants(cmd, mColoredPipeline.Layout,
                                   VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(transform),
                                   &transform);
                vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
            }
        }
    }
    vkCmdEndRendering(cmd);
}

void Minimal3DRenderer::CreateSwapchainResources()
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

    // Create depth buffer:
    ImageInfo depthBufferInfo{.Extent = drawExtent,
                              .Format = mDepthFormat,
                              .Tiling = VK_IMAGE_TILING_OPTIMAL,
                              .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};

    mDepthBuffer = Image::CreateImage2D(mCtx, depthBufferInfo);
    mDepthBufferView =
        Image::CreateView2D(mCtx, mDepthBuffer, mDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    mSwapchainDeletionQueue.push_back(&mDepthBuffer);
    mSwapchainDeletionQueue.push_back(mDepthBufferView);
}

void Minimal3DRenderer::LoadScene(Scene &scene)
{
    if (scene.GlobalUpdate)
    {
        // This will only take efect on non-first runs:
        mSceneDeletionQueue.flush();
        mColoredDrawables.clear();
        mTexturedDrawables.clear();
        mTextures.clear();

        LoadProviders(scene);
        LoadTextures(scene);
    }

    LoadInstances(scene);
}

void Minimal3DRenderer::LoadProviders(Scene &scene)
{
    auto &pool = mFrame.CurrentPool();

    for (auto &obj : scene.Objects)
    {
        if (!mColoredLayout.IsCompatible(obj.Provider.Layout))
        {
            continue;
        }

        mColoredDrawables.emplace_back();
        auto &drawable = mColoredDrawables.back();

        auto geo = obj.Provider.GetGeometry();

        // Create Vertex buffer:
        drawable.VertexBuffer =
            VertexBuffer::Create(mCtx, mQueues.Graphics, pool, geo.VertexData);
        drawable.VertexCount = geo.VertexData.Count;

        // Create Index buffer:
        drawable.IndexBuffer = IndexBuffer::Create(mCtx, mQueues.Graphics, pool, geo.IndexData);
        drawable.IndexCount = geo.IndexData.Count;
    }

    for (auto &obj : scene.Objects)
    {
        if (!mTexturedLayout.IsCompatible(obj.Provider.Layout))
        {
            continue;
        }

        mTexturedDrawables.emplace_back();
        auto &drawable = mTexturedDrawables.back();

        auto geo = obj.Provider.GetGeometry();

        // Create Vertex buffer:
        drawable.VertexBuffer =
            VertexBuffer::Create(mCtx, mQueues.Graphics, pool, geo.VertexData);
        drawable.VertexCount = geo.VertexData.Count;

        // Create Index buffer:
        drawable.IndexBuffer = IndexBuffer::Create(mCtx, mQueues.Graphics, pool, geo.IndexData);
        drawable.IndexCount = geo.IndexData.Count;

        // Retrieve texture id
        if (obj.TextureId.has_value())
            drawable.TextureId = obj.TextureId.value();
    }

    for (auto &drawable : mColoredDrawables)
    {
        mSceneDeletionQueue.push_back(&drawable.VertexBuffer);
        mSceneDeletionQueue.push_back(&drawable.IndexBuffer);
    }

    for (auto &drawable : mTexturedDrawables)
    {
        mSceneDeletionQueue.push_back(&drawable.VertexBuffer);
        mSceneDeletionQueue.push_back(&drawable.IndexBuffer);
    }
}

void Minimal3DRenderer::LoadTextures(Scene &scene)
{
    auto &pool = mFrame.CurrentPool();

    // Create images and views, allocate descriptor sets:
    for (const auto &path : scene.Textures.data())
    {
        mTextures.emplace_back();
        auto &texture = mTextures.back();

        texture.TexImage = ImageLoaders::LoadImage2D(mCtx, mQueues.Graphics, pool, path);

        auto format = texture.TexImage.Info.Format;
        texture.View = Image::CreateView2D(mCtx, texture.TexImage, format,
                                           VK_IMAGE_ASPECT_COLOR_BIT);

        // To-do: If deletion queue is updated directly here, image handle of the
        // first texture gets corrupted, investigate why:
        // mSceneDeletionQueue.push_back(&texture.TexImage);
        // mSceneDeletionQueue.push_back(texture.View);

        // To-do: add variant of allocate that allocates singular descriptor set:
        std::array<VkDescriptorSetLayout, 1> layouts{mTextureDescriptorSetLayout};
        texture.DescriptorSet =
            Descriptor::Allocate(mCtx, mTextureDescriptorPool, layouts)[0];
    }

    // Update descriptor sets:
    for (const auto &texture : mTextures)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texture.View;
        imageInfo.sampler = mSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = texture.DescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(mCtx.Device, 1, &descriptorWrite, 0, nullptr);
    }

    for (auto &texture : mTextures)
    {
        mSceneDeletionQueue.push_back(&texture.TexImage);
        mSceneDeletionQueue.push_back(texture.View);
    }
}

void Minimal3DRenderer::LoadInstances(Scene &scene)
{
    size_t drawableId = 0;

    for (auto &obj : scene.Objects)
    {
        if (!mColoredLayout.IsCompatible(obj.Provider.Layout))
        {
            continue;
        }

        auto &drawable = mColoredDrawables[drawableId];

        if (obj.UpdateInstances)
        {
            drawable.Transforms.clear();

            // Unpack instance data:
            for (auto instance : obj.Instances)
            {
                drawable.Transforms.push_back(instance.GetTransform());
            }
        }

        drawableId++;
    }

    drawableId = 0;

    for (auto &obj : scene.Objects)
    {
        if (!mTexturedLayout.IsCompatible(obj.Provider.Layout))
        {
            continue;
        }

        auto &drawable = mTexturedDrawables[drawableId];

        if (obj.UpdateInstances)
        {
            drawable.Transforms.clear();

            // Unpack instance data:
            for (auto instance : obj.Instances)
            {
                drawable.Transforms.push_back(instance.GetTransform());
            }
        }

        drawableId++;
    }
}
