#include "ViewHandler.h"

#include "BufferUtils.h"
#include "Descriptor.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

ViewHandler::ViewHandler(VulkanContext &ctx, FrameInfo &frame)
    : mFrame(frame), mDeletionQueue(ctx)
{
    // Create descriptor sets:
    //  Descriptor layout
    mDescriptorSetLayout =
        DescriptorSetLayoutBuilder("CameraDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .Build(ctx, mDeletionQueue);

    // Descriptor pool
    uint32_t maxSets = mFrame.MaxInFlight;

    std::vector<VkDescriptorPoolSize> poolCounts{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets},
    };

    mDescriptorPool = Descriptor::InitPool(ctx, maxSets, poolCounts);
    mDeletionQueue.push_back(mDescriptorPool);

    // Descriptor sets allocation
    std::vector<VkDescriptorSetLayout> layouts(mFrame.MaxInFlight, mDescriptorSetLayout);

    mDescriptorSets = Descriptor::Allocate(ctx, mDescriptorPool, layouts);

    // Create Uniform Buffers:
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    mUniformBuffers.resize(mFrame.MaxInFlight);

    for (auto &uniformBuffer : mUniformBuffers)
    {
        uniformBuffer = MakeBuffer::MappedUniform(ctx, "CameraUniformBuffer", bufferSize);
        mDeletionQueue.push_back(uniformBuffer);
    }

    // Update descriptor sets:
    for (size_t i = 0; i < mDescriptorSets.size(); i++)
    {
        DescriptorUpdater(mDescriptorSets[i])
            .WriteUniformBuffer(0, mUniformBuffers[i].Handle, sizeof(UniformBufferObject))
            .Update(ctx);
    }
}

ViewHandler::~ViewHandler()
{
    mDeletionQueue.flush();
}

void ViewHandler::OnUpdate(glm::mat4 camViewProj, glm::vec3 lightDir)
{
    // Calculate light view-proj
    float near_plane = 0.0f, far_plane = 20.0f;
    glm::mat4 proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
    glm::mat4 view = glm::lookAt(10.0f * glm::normalize(lightDir), glm::vec3(0.0f), glm::vec3(0, -1, 0));

    // To compensate for change of orientation between
    // OpenGL and Vulkan:
    proj[1][1] *= -1;

    // Upload data to uniform buffer:
    mUBOData.CameraViewProjection = camViewProj;
    mUBOData.LightViewProjection = proj * view;

    auto &uniformBuffer = mUniformBuffers[mFrame.Index];
    Buffer::UploadToMapped(uniformBuffer, &mUBOData, sizeof(mUBOData));
}