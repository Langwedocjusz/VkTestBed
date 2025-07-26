#pragma once

#include "Buffer.h"
#include "DeletionQueue.h"
#include "Frame.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

class ViewHandler {
  public:
    ViewHandler(VulkanContext &ctx, FrameInfo &frame);
    ~ViewHandler();

    void OnUpdate(glm::mat4 camViewProj, glm::vec3 lightDir);

    [[nodiscard]] VkDescriptorSetLayout DescriptorSetLayout() const
    {
        return mDescriptorSetLayout;
    }

    // To-do: figure out a better way of doing this:
    [[nodiscard]] VkDescriptorSet *DescriptorSet()
    {
        return &mDescriptorSets[mFrame.ImageIndex];
    }

  private:
    VkDescriptorSetLayout mDescriptorSetLayout;
    VkDescriptorPool mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;

    struct UniformBufferObject {
        glm::mat4 CameraViewProjection = glm::mat4(1.0f);
        glm::mat4 LightViewProjection = glm::mat4(1.0f);
    } mUBOData;

    std::vector<Buffer> mUniformBuffers;

    FrameInfo &mFrame;
    DeletionQueue mDeletionQueue;
};