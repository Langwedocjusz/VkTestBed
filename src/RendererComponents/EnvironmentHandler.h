#pragma once

#include "DeletionQueue.h"
#include "Descriptor.h"
#include "Pipeline.h"
#include "RenderContext.h"
#include "Scene.h"
#include "VulkanContext.h"

#include <glm/glm.hpp>

class EnvironmentHandler {
  public:
    EnvironmentHandler(VulkanContext &ctx, FrameInfo &info,
                       RenderContext::Queues &queues);
    ~EnvironmentHandler();

    void RebuildPipelines();
    void LoadEnvironment(const Scene &scene);

    [[nodiscard]] bool HdriEnabled() const
    {
        return mEnvUBOData.HdriEnabled;
    }
    [[nodiscard]] VkDescriptorSet *GetDescriptorSetPtr()
    {
        return &mEnvironmentDescriptorSet;
    }
    [[nodiscard]] VkDescriptorSetLayout GetDescriptorLayout() const
    {
        return mEnvironmentDescriptorSetLayout;
    }

  private:
    VulkanContext &mCtx;
    FrameInfo &mFrame;
    RenderContext::Queues &mQueues;

    DescriptorAllocator mDescriptorAllocator;

    VkDescriptorSetLayout mEnvironmentDescriptorSetLayout;
    VkDescriptorSet mEnvironmentDescriptorSet;

    std::optional<SceneKey> mLastHdri;

    VkDescriptorSetLayout mCubeGenDescriptorSetLayout;
    VkDescriptorSet mCubeGenDescriptorSet;

    Pipeline mEquiRectToCubePipeline;

    struct CubeGenPCData {
        int32_t SideId;
    };

    Image mCubemap;
    VkImageView mCubemapView;
    VkSampler mSampler;

    struct EnvUBOData {
        // Fourth component indicates whether the light is on or not
        glm::vec4 LightDir = glm::vec4(glm::normalize(glm::vec3(1, -1, 1)), 1);
        int32_t HdriEnabled = false;
    } mEnvUBOData;

    Buffer mEnvUBO;

    DeletionQueue mDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};