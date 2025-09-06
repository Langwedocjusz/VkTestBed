#pragma once

#include "DeletionQueue.h"
#include "Descriptor.h"
#include "Pipeline.h"
#include "Scene.h"
#include "Texture.h"
#include "VulkanContext.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

class EnvironmentHandler {
  public:
    struct EnvUBOData {
        int32_t LightOn = 1;
        glm::vec3 LightDir = glm::normalize(glm::vec3(1, -1, 1));
        glm::vec3 LightColor = glm::vec3(1.0);
        int32_t HdriEnabled = false;
        float MaxReflectionLod = 0.0f;
    };

  public:
    EnvironmentHandler(VulkanContext &ctx);
    ~EnvironmentHandler();

    void RebuildPipelines();
    void LoadEnvironment(const Scene &scene);

    [[nodiscard]] bool HdriEnabled() const
    {
        return mEnvUBOData.HdriEnabled;
    }

    [[nodiscard]] EnvUBOData GetUboData() const
    {
        return mEnvUBOData;
    };

    [[nodiscard]] VkDescriptorSet *GetBackgroundDSPtr()
    {
        return &mBackgroundDescriptorSet;
    }
    [[nodiscard]] VkDescriptorSetLayout GetBackgroundDSLayout() const
    {
        return mBackgroundDescrptorSetLayout;
    }

    [[nodiscard]] VkDescriptorSet *GetLightingDSPtr()
    {
        return &mLightingDescriptorSet;
    }
    [[nodiscard]] VkDescriptorSetLayout GetLightingDSLayout() const
    {
        return mLightingDescriptorSetLayout;
    }

  private:
    void ConvertEquirectToCubemap(const ImageData &data);

    void CalculateDiffuseIrradiance();
    void GeneratePrefilteredMap();
    void GenerateIntegrationMap();

    void ResetToBlack();

  private:
    VulkanContext &mCtx;

    // Descriptor sets exposed to the outside world:
    VkDescriptorSetLayout mLightingDescriptorSetLayout;
    VkDescriptorSet mLightingDescriptorSet;

    VkDescriptorSetLayout mBackgroundDescrptorSetLayout;
    VkDescriptorSet mBackgroundDescriptorSet;

    // Private descriptor sets:

    // Descriptor set for generating the cubemap:
    VkDescriptorSetLayout mTexToImgDescriptorSetLayout;
    VkDescriptorSet mTexToImgDescriptorSet;

    // Descriptor set for irradiance reduction buffers:
    VkDescriptorSetLayout mIrradianceDescriptorSetLayout;
    VkDescriptorSet mIrradianceDescriptorSet;

    // Descriptor set for generating the prefiltered map:
    VkDescriptorSetLayout mPrefilteredDescriptorSetLayout;
    VkDescriptorSet mPrefilteredDescriptorSet;

    // Descriptor set for generating the integration map:
    VkDescriptorSetLayout mIntegrationDescriptorSetLayout;
    VkDescriptorSet mIntegrationDescriptorSet;

    // Compute pipelines for resource generation:
    Pipeline mEquiRectToCubePipeline;

    struct IrradianceSHPushConstants {
        uint32_t CubemapRes;
        uint32_t ReduceBlock;
    };

    struct ReducePushConstants {
        uint32_t BufferSize;
    };

    Pipeline mIrradianceSHPipeline;
    Pipeline mIrradianceReducePipeline;

    struct PrefilteredPushConstants {
        uint32_t CubeResolution;
        uint32_t MipLevel;
        float Roughness;
    };

    Pipeline mPrefilteredGenPipeline;
    Pipeline mIntegrationGenPipeline;

    // SSBOs for reduction when computing SH irradiance coefficients:
    Buffer mFirstReducionBuffer;
    Buffer mFinalReductionBuffer;

    uint32_t mReduceBlock = 32;
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
    Buffer mEnvUBO;

    // std::optional<SceneKey> mLastHdri;
    DescriptorAllocator mDescriptorAllocator;
    DeletionQueue mDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};