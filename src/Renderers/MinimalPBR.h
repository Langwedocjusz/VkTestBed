#pragma once

#include "DeletionQueue.h"
#include "Descriptor.h"
#include "DynamicUniformBuffer.h"
#include "EnvironmentHandler.h"
#include "GeometryData.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Scene.h"
#include "Texture.h"
#include <vulkan/vulkan_core.h>

class MinimalPbrRenderer final : public IRenderer {
  public:
    MinimalPbrRenderer(VulkanContext &ctx, FrameInfo &info, Camera &camera);
    ~MinimalPbrRenderer();

    void OnUpdate([[maybe_unused]] float deltaTime) override;
    void OnImGui() override;
    void OnRender() override;

    void CreateSwapchainResources() override;
    void RebuildPipelines() override;
    void LoadScene(const Scene &scene) override;

  private:
    void LoadMeshes(const Scene &scene);
    void LoadImages(const Scene &scene);
    void LoadMaterials(const Scene &scene);
    void LoadMeshMaterials(const Scene &scene);
    void LoadObjects(const Scene &scene);

    struct DrawStats {
        uint32_t NumIdx = 0;
        uint32_t NumDraws = 0;
        uint32_t NumBinds = 0;
    };

    void ShadowPass(VkCommandBuffer cmd, DrawStats &stats);
    void MainPass(VkCommandBuffer cmd, DrawStats &stats);

    struct Drawable;
    void Draw(VkCommandBuffer cmd, Drawable &drawable, bool shadowPass, DrawStats &stats);

  private:
    // Framebuffer related things:
    const float mInternalResolutionScale = 1.0f;
    const VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;

    const VkFormat mDepthFormat = VK_FORMAT_D32_SFLOAT;
    Image mDepthBuffer;
    VkImageView mDepthBufferView;

    // Common resources:
    VkSampler mSampler2D;
    VkSampler mSamplerShadowmap;

    // Shadow and main material pass:
    Pipeline mShadowmapPipeline;
    Pipeline mMainPipeline;

    Image mShadowmap;
    VkImageView mShadowmapView;

    VkDescriptorSetLayout mShadowmapDescriptorSetLayout;
    VkDescriptorSet mShadowmapDescriptorSet;
    // To-do: It's overkill to use the allocator for this
    DescriptorAllocator mShadowmapDescriptorAllocator;

    using enum Vertex::AttributeType;

    GeometryLayout mGeometryLayout{
        .VertexLayout = {Vec3, Vec2, Vec3, Vec4},
        .IndexType = VK_INDEX_TYPE_UINT32,
    };

    struct ShadowPCData {
        glm::mat4 LightMVP;
    };

    struct MainPCData {
        glm::mat4 Model;
        // In principle this could be a mat3, but that somehow
        // breaks the push constant data layout...
        glm::mat4 Normal;
    };

    Texture mDefaultAlbedo;
    Texture mDefaultRoughness;
    Texture mDefaultNormal;

    std::map<SceneKey, Texture> mImages;

    VkDescriptorSetLayout mMaterialDescriptorSetLayout;
    DescriptorAllocator mMaterialDescriptorAllocator;

    struct Material {
        VkDescriptorSet DescriptorSet;

        struct {
            float AlphaCutoff = 0.5f;
            glm::vec3 TranslucentColor = glm::vec3(0.0f);
            int32_t DoubleSided = 0;
        } UboData;

        Buffer UBO;
    };

    std::map<SceneKey, Material> mMaterials;

    struct Drawable {
        Buffer VertexBuffer;
        uint32_t VertexCount;

        Buffer IndexBuffer;
        uint32_t IndexCount;

        BoundingBox Bbox;
        SceneKey MaterialKey = 0;
        std::vector<glm::mat4> Instances;
    };

    // Drawables correspond to mesh primitives
    // so DrawableKey is the pair (MeshKey, PrimitiveId)
    using DrawableKey = std::pair<SceneKey, size_t>;
    std::map<DrawableKey, Drawable> mDrawables;

    std::vector<DrawableKey> mSingleSidedDrawableKeys;
    std::vector<DrawableKey> mDoubleSidedDrawableKeys;

    // Dynamic uniform data including camera/lighting:
    struct UBOData {
        glm::mat4 CameraViewProjection;
        glm::mat4 LightViewProjection;
        glm::vec3 ViewPos;
        float DirectionalFactor = 3.0f;
        float EnvironmentFactor = 0.05f;
        float ShadowBiasMin = 0.001f;
        float ShadowBiasMax = 0.005f;
    } mUBOData;

    DynamicUniformBuffer mDynamicUBO;

    // Cubemap generation and background drawing:
    EnvironmentHandler mEnvHandler;

    Pipeline mBackgroundPipeline;

    struct FrustumData {
        glm::vec4 TopLeft;
        glm::vec4 TopRight;
        glm::vec4 BottomLeft;
        glm::vec4 BottomRight;
    } mFrustumData;

    float mAddZ = 8.0f;
    float mSubZ = 20.0f;

    VkDescriptorSet mDebugTextureDescriptorSet;

    float mShadowDist = 20.0f;

    // Deletion queues:
    DeletionQueue mSceneDeletionQueue;
    DeletionQueue mMaterialDeletionQueue;
};