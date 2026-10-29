#pragma once

#include "Buffer.h"
#include "Camera.h"
#include "DeletionQueue.h"
#include "Image.h"
#include "Pipeline.h"
#include "Scene.h"
#include "VulkanContext.h"

#include "volk.h"
#include <glm/glm.hpp>

class EnvironmentHandler {
  public:
    struct EnvUBOData {
        int32_t   LightOn          = 1;
        glm::vec3 LightDir         = glm::normalize(glm::vec3(1, -1, 1));
        glm::vec3 LightColor       = glm::vec3(1.0f);
        int32_t   HdriEnabled      = false;
        float     MaxReflectionLod = 0.0f;
    };

  public:
    EnvironmentHandler(VulkanContext &ctx, VkDescriptorPool staticPool);

    void RebuildPipelines(VkFormat colorFormat, VkFormat depthFormat, VkSampleCountFlagBits sampleCount);
    void LoadEnvironment(const Scene &scene);

    void DrawBackground(VkCommandBuffer cmd, FrustumBack f, VkExtent2D drawExtent);

    // Retrieve descriptor set (and its layout)
    // for rendering objects (indirect lighting/reflections):
    [[nodiscard]] VkDescriptorSet GetLightingDS() const
    {
        return mLightingDescriptorSet;
    }
    [[nodiscard]] VkDescriptorSetLayout GetLightingDSLayout() const
    {
        return mLightingDescriptorSetLayout;
    }

    // Retrieve auxiliary information about the environment
    // that may be useful for other components:
    [[nodiscard]] bool HdriEnabled() const
    {
        return mEnvUBOData.HdriEnabled;
    }

    [[nodiscard]] glm::vec3 GetLightDir() const
    {
        return mEnvUBOData.LightDir;
    }

  private:
    void ConvertEquirectToCubemap(const ImageData &data);

    void CalculateDiffuseIrradiance();
    void GeneratePrefilteredMap();
    void GenerateIntegrationMap();

    void ResetToBlack();

  private:
    static constexpr uint32_t CubemapSize     = 1024;
    static constexpr uint32_t PrefilteredSize = 256;
    static constexpr uint32_t IntegrationSize = 512;

    VulkanContext   &mCtx;
    VkDescriptorPool mStaticPool;

    // Descriptor set exposed to the outside world:
    VkDescriptorSetLayout mLightingDescriptorSetLayout;
    VkDescriptorSet       mLightingDescriptorSet;

    // Compute pipelines for resource generation:
    ComputePipeline mEquiRectToCubePipeline;
    ComputePipeline mIrradianceSHPipeline;
    ComputePipeline mIrradianceReducePipeline;
    ComputePipeline mPrefilteredGenPipeline;
    ComputePipeline mIntegrationGenPipeline;

    // Pipeline for drawing the background:
    GraphicsPipeline mBackgroundPipeline;

    // SSBOs for reduction when computing SH irradiance coefficients:
    std::optional<Buffer> mFirstReducionBuffer;
    std::optional<Buffer> mFinalReductionBuffer;

    uint32_t mReduceBlock    = 32;
    uint32_t mFirstBufferLen = 0;

    // Cubemap background texture:
    std::optional<Texture> mCubemap;
    std::optional<Texture> mPrefiltered;
    std::optional<Texture> mIntegration;

    std::vector<VkImageView> mPrefilteredMipViews;

    VkSampler mSampler;
    VkSampler mSamplerClamped;
    VkSampler mSamplerMipped;

    // Uniform buffer object with environment info for lighting:
    EnvUBOData            mEnvUBOData;
    std::optional<Buffer> mEnvUBO;

    DeletionQueue mDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};