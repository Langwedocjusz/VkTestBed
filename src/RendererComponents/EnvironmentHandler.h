#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Descriptor.h"
#include "Pipeline.h"
#include "Scene.h"
#include "Texture.h"
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
    EnvironmentHandler(VulkanContext &ctx);

    void RebuildPipelines(VkFormat colorFormat, VkFormat depthFormat,
                          VkSampleCountFlagBits sampleCount);
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

    VulkanContext &mCtx;

    bool mIntegrationGenerated = false;

    // Descriptor set exposed to the outside world:
    VkDescriptorSetLayout mLightingDescriptorSetLayout;
    VkDescriptorSet       mLightingDescriptorSet;

    // Private descriptor sets:

    // Descriptor set for generating the cubemap:
    VkDescriptorSetLayout mTexToImgDescriptorSetLayout;
    VkDescriptorSet       mTexToImgDescriptorSet;

    // Descriptor set for irradiance reduction buffers:
    VkDescriptorSetLayout mIrradianceDescriptorSetLayout;
    VkDescriptorSet       mIrradianceDescriptorSet;

    // Descriptor set for generating the prefiltered map:
    VkDescriptorSetLayout mPrefilteredDescriptorSetLayout;
    VkDescriptorSet       mPrefilteredDescriptorSet;

    // Descriptor set for generating the integration map:
    VkDescriptorSetLayout mIntegrationDescriptorSetLayout;
    VkDescriptorSet       mIntegrationDescriptorSet;

    // Descriptor set for drawing the background:
    VkDescriptorSetLayout mBackgroundDescrptorSetLayout;
    VkDescriptorSet       mBackgroundDescriptorSet;

    // Push constants data layouts for pipelines:
    struct PCDataIrradianceSH {
        uint32_t CubemapRes;
        uint32_t ReduceBlock;
    };

    struct PCDataReduce {
        uint32_t BufferSize;
    };

    struct PCDataPrefiltered {
        uint32_t CubeResolution;
        uint32_t MipLevel;
        float    Roughness;
    };

    // Compute pipelines for resource generation:
    Pipeline mEquiRectToCubePipeline;
    Pipeline mIrradianceSHPipeline;
    Pipeline mIrradianceReducePipeline;
    Pipeline mPrefilteredGenPipeline;
    Pipeline mIntegrationGenPipeline;

    // Pipeline for drawing the background:
    Pipeline mBackgroundPipeline;

    // SSBOs for reduction when computing SH irradiance coefficients:
    Buffer mFirstReducionBuffer;
    Buffer mFinalReductionBuffer;

    uint32_t mReduceBlock    = 32;
    uint32_t mFirstBufferLen = 0;

    // Cubemap background texture:
    Texture mCubemap;
    Texture mPrefiltered;
    Texture mIntegration;

    std::vector<VkImageView> mPrefilteredMipViews;

    VkSampler mSampler;
    VkSampler mSamplerClamped;
    VkSampler mSamplerMipped;

    // Uniform buffer object with environment info for lighting:
    EnvUBOData mEnvUBOData;
    Buffer     mEnvUBO;

    VkDescriptorPool mStaticDescriptorPool;

    DeletionQueue mDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};