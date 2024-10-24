#include "Camera.h"

#include "Descriptor.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(VulkanContext &ctx, FrameInfo &info)
    : mCtx(ctx), mFrame(info), mMainDeletionQueue(ctx)
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
}

Camera::~Camera()
{
    mMainDeletionQueue.flush();
}

void Camera::OnUpdate()
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