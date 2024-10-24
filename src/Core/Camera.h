#pragma once

#include "Buffer.h"
#include "DeletionQueue.h"
#include "Frame.h"
#include "VulkanContext.h"
#include <glm/glm.hpp>

class Camera {
  public:
    Camera(VulkanContext &ctx, FrameInfo &info);
    ~Camera();

    void OnUpdate();

    [[nodiscard]] VkDescriptorSetLayout DescriptorSetLayout() const
    {
        return mDescriptorSetLayout;
    }

    //To-do: figure out a better way of doing this:
    [[nodiscard]] VkDescriptorSet* DescriptorSet()
    {
        return &mDescriptorSets[mFrame.Index];
    }

  private:
    VulkanContext &mCtx;
    FrameInfo &mFrame;

    VkDescriptorSetLayout mDescriptorSetLayout;
    VkDescriptorPool mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;

    struct UniformBufferObject {
        glm::mat4 ViewProjection = glm::mat4(1.0f);
    };
    UniformBufferObject mUBOData;

    std::vector<Buffer> mUniformBuffers;

    DeletionQueue mMainDeletionQueue;
};