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
    void OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj) override;

    void CreateSwapchainResources() override;
    void RebuildPipelines() override;
    void LoadScene(const Scene &scene) override;
    void RenderObjectId(VkCommandBuffer cmd, float x, float y) override;

  private:
    struct DrawStats {
        uint32_t NumIdx = 0;
        uint32_t NumDraws = 0;
        uint32_t NumBinds = 0;
    };

    struct Material {
        VkDescriptorSet DescriptorSet;

        struct {
            float AlphaCutoff = 0.5f;
            glm::vec3 TranslucentColor = glm::vec3(0.0f);
            int32_t DoubleSided = 0;
        } UboData;

        Buffer UBO;
    };

    struct Instance {
        SceneKey ObjectId;
        glm::mat4 Transform;
    };

    struct Drawable {
        Buffer VertexBuffer;
        uint32_t VertexCount;

        Buffer IndexBuffer;
        uint32_t IndexCount;

        BoundingBox Bbox;
        SceneKey MaterialKey = 0;
        std::vector<Instance> Instances;
    };

    // Drawables correspond to mesh primitives
    // so DrawableKey is the pair (MeshKey, PrimitiveId)
    using DrawableKey = std::pair<SceneKey, size_t>;

    using PCSetFunction = std::function<void(Instance &)>;

    struct DrawSettings {
        glm::mat4 ViewProj;
        VkPipelineLayout Layout;
        void *PCData;
        size_t PCSize;
        PCSetFunction PCSetFn;
    };

  private:
    void LoadMeshes(const Scene &scene);
    void LoadImages(const Scene &scene);
    void LoadMaterials(const Scene &scene);
    void LoadMeshMaterials(const Scene &scene);
    void LoadObjects(const Scene &scene);

    void ShadowPass(VkCommandBuffer cmd, DrawStats &stats);
    void MainPass(VkCommandBuffer cmd, DrawStats &stats);
    void OutlinePass(VkCommandBuffer cmd, SceneKey highlightedObj);

    void DrawAllInstances(VkCommandBuffer cmd, Drawable &drawable, DrawSettings &settings,
                          DrawStats &stats);
    void DrawSingleInstanceOutline(VkCommandBuffer cmd, DrawableKey drawableKey,
                                   size_t instanceId, VkPipelineLayout layout);

    void DestroyDrawable(const Drawable &drawable);
    void DestroyTexture(const Texture &texture);

  private:
    // Framebuffer:
    static constexpr VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;
    static constexpr VkFormat mDepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
    static constexpr VkFormat mShadowmapFormat = VK_FORMAT_D32_SFLOAT;

    Image mDepthStencilBuffer;
    VkImageView mDepthStencilBufferView;

    // Common resources:
    VkSampler mSampler2D;
    VkDescriptorPool mStaticDescriptorPool;

    // Graphics pipelines:
    Pipeline mShadowmapPipeline;
    Pipeline mMainPipeline;
    Pipeline mBackgroundPipeline;
    Pipeline mStencilPipeline;
    Pipeline mOutlinePipeline;
    Pipeline mObjectIdPipeline;

    // Push-constant structs for all pipelines:
    struct {
        glm::mat4 LightMVP;
    } mShadowPCData;

    struct {
        glm::mat4 Model;
        // In principle this could be a mat3, but that somehow
        // breaks the push constant data layout...
        glm::mat4 Normal;
    } mMainPCData;

    struct {
        glm::mat4 Model;
    } mOutlinePCData;

     struct {
         glm::mat4 Model;
         uint32_t ObjectId;
     } mObjectIdPCData;

    // Images for materials:
    Texture mDefaultAlbedo;
    Texture mDefaultRoughness;
    Texture mDefaultNormal;
    std::map<SceneKey, Texture> mImages;

    // Descriptors for materials:s
    VkDescriptorSetLayout mMaterialDescriptorSetLayout;
    DynamicDescriptorAllocator mMaterialDescriptorAllocator;
    std::map<SceneKey, Material> mMaterials;

    // Drawables:
    using enum Vertex::AttributeType;

    GeometryLayout mGeometryLayout{
        .VertexLayout = {Vec3, Vec2, Vec3, Vec4},
        .IndexType = VK_INDEX_TYPE_UINT32,
    };

    std::map<DrawableKey, Drawable> mDrawables;

    // Drawable keys separated by wheter the underlying material is double sided:
    std::vector<DrawableKey> mSingleSidedDrawableKeys;
    std::vector<DrawableKey> mDoubleSidedDrawableKeys;

    // Drawables whose outline should be drawn:
    std::optional<SceneKey> mLastHighlightedObjKey = std::nullopt;
    // size_t in pair is transform id:
    std::vector<std::pair<DrawableKey, size_t>> mSelectedDrawableKeys;

    // Index cache for retrieving drawables and transform ids based on object id:
    std::map<SceneKey, std::vector<std::pair<DrawableKey, size_t>>> mObjectCache;

    // Some renderer settings:
    const float mInternalResolutionScale = 1.0f;
    float mAddZ = 8.0f;
    float mSubZ = 20.0f;
    float mShadowDist = 20.0f;

    // Dynamic uniform data including camera/lighting and more renderer settings:
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

    // Shadow map:
    Image mShadowmap;
    VkImageView mShadowmapView;
    VkSampler mSamplerShadowmap;

    VkDescriptorSetLayout mShadowmapDescriptorSetLayout;
    VkDescriptorSet mShadowmapDescriptorSet;

    // Cubemap generation and background drawing:
    EnvironmentHandler mEnvHandler;

    // Descriptor set for sending shadow map view to imgui:
    VkDescriptorSet mDebugTextureDescriptorSet;

    // Deletion queues:
    DeletionQueue mSceneDeletionQueue;
    DeletionQueue mMaterialDeletionQueue;
};