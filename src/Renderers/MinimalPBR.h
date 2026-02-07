#pragma once

#include "DeletionQueue.h"
#include "Descriptor.h"
#include "DynamicUniformBuffer.h"
#include "EnvironmentHandler.h"
#include "GeometryData.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Scene.h"
#include "ShadowmapHandler.h"
#include "Texture.h"
#include "VulkanContext.h"

class MinimalPbrRenderer final : public IRenderer {
  public:
    MinimalPbrRenderer(VulkanContext &ctx, FrameInfo &info, Camera &camera);
    ~MinimalPbrRenderer();

    void OnUpdate([[maybe_unused]] float deltaTime) override;
    void OnImGui() override;
    void OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj) override;

    void RecreateSwapchainResources() override;
    void RebuildPipelines() override;
    void LoadScene(const Scene &scene) override;
    void RenderObjectId(VkCommandBuffer cmd, float x, float y) override;

  private:
    struct DrawStats {
        uint32_t NumIdx   = 0;
        uint32_t NumDraws = 0;
        uint32_t NumBinds = 0;
    };

    struct Material {
        VkDescriptorSet DescriptorSet;
        Buffer          UBO;

        struct {
            float     AlphaCutoff      = 0.5f;
            glm::vec3 TranslucentColor = glm::vec3(0.0f);
            int32_t   DoubleSided      = 0;
        } UboData;
    };

    struct Instance {
        SceneKey  ObjectId;
        glm::mat4 Transform;
    };

    // TODO: This will be remade in RAII fashion when VulkanContext is reworked as a
    // singleton. Currently each drawable would need to hold a reference to it, which
    // aside from memory usage also makes it difficult to make drawables movable-non
    // copyable, as they should be.
    struct Drawable {
        void Init(VulkanContext &ctx, const ScenePrimitive &prim,
                  const std::string &debugName);
        void Destroy(VulkanContext &ctx);

        bool EarlyBail(glm::mat4 viewProj);
        bool IsVisible(glm::mat4 viewProj, size_t instanceIdx);
        void BindGeometryBuffers(VkCommandBuffer cmd);
        void Draw(VkCommandBuffer cmd);

        Buffer   VertexBuffer;
        uint32_t VertexCount;

        Buffer   IndexBuffer;
        uint32_t IndexCount;

        AABB                  Bbox;
        SceneKey              MaterialKey = 0;
        std::vector<Instance> Instances;
    };

    // Drawables correspond to mesh primitives
    // so DrawableKey is the pair (MeshKey, PrimitiveId)
    using DrawableKey = std::pair<SceneKey, size_t>;

  private:
    void LoadMeshes(const Scene &scene);
    void LoadImages(const Scene &scene);
    void LoadMaterials(const Scene &scene);
    void LoadMeshMaterials(const Scene &scene);
    void LoadObjects(const Scene &scene);

    void ShadowPass(VkCommandBuffer cmd, DrawStats &stats);
    void Prepass(VkCommandBuffer cmd, DrawStats &stats);
    void AOPass(VkCommandBuffer cmd, DrawStats &stats);
    void MainPass(VkCommandBuffer cmd, DrawStats &stats);
    void OutlinePass(VkCommandBuffer cmd, SceneKey highlightedObj);

    void DestroyTexture(const Texture &texture);

    template <typename MaterialFn, typename InstanceFn>
    void DrawAllInstancesCulled(VkCommandBuffer cmd, Drawable &drawable,
                                glm::mat4 viewProj, MaterialFn materialCallback,
                                InstanceFn instanceCallback, DrawStats &stats);

    template <typename MaterialFn, typename InstanceFn>
    void DrawSingleSidedFrustumCulled(VkCommandBuffer cmd, glm::mat4 viewProj,
                                      MaterialFn materialCallback,
                                      InstanceFn instanceCallback, DrawStats &stats);

    template <typename MaterialFn, typename InstanceFn>
    void DrawDoubleSidedFrustumCulled(VkCommandBuffer cmd, glm::mat4 viewProj,
                                      MaterialFn materialCallback,
                                      InstanceFn instanceCallback, DrawStats &stats);

    template <typename MaterialFn, typename InstanceFn>
    void DrawSceneFrustumCulled(VkCommandBuffer cmd, glm::mat4 viewProj,
                                MaterialFn materialCallback, InstanceFn instanceCallback,
                                DrawStats &stats);

  private:
    // Framebuffer:
    static constexpr VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;
    static constexpr VkFormat mDepthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

    std::optional<Texture> mRenderTargetMsaa;
    std::optional<Texture> mDepthStencilMsaa;

    Texture     mDepthStencilBuffer;
    VkImageView mDepthOnlyView;
    Texture     mAOTarget;

    // Common resources:
    VkSampler mSampler2D;

    // Graphics pipelines:
    Pipeline mZPrepassOpaquePipeline;
    Pipeline mZPrepassAlphaPipeline;
    Pipeline mAOGenPipeline;
    Pipeline mMainPipeline;
    Pipeline mBackgroundPipeline;
    Pipeline mStencilPipeline;
    Pipeline mOutlinePipeline;
    Pipeline mObjectIdPipeline;

    // Push-constant structs for all pipelines:
    struct {
        glm::mat4 Model;
    } mPrepassPCData;

    struct {
        glm::mat4 Proj;
        glm::mat4 InvProj;
    } mAOGenPCData;

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
        uint32_t  ObjectId;
    } mObjectIdPCData;

    // Images for materials:
    Texture                     mDefaultAlbedo;
    Texture                     mDefaultRoughness;
    Texture                     mDefaultNormal;
    std::map<SceneKey, Texture> mImages;

    // Descriptors for materials:
    VkDescriptorSetLayout        mMaterialDescriptorSetLayout;
    DynamicDescriptorAllocator   mMaterialDescriptorAllocator;
    std::map<SceneKey, Material> mMaterials;

    // AO descriptors:
    VkDescriptorPool mAODescriptorPool;
    // Generation:
    VkDescriptorSetLayout mAOGenDescriptorSetLayout;
    VkDescriptorSet       mAOGenDescriptorSet;
    // Usage:
    VkDescriptorSetLayout mAOUsageDescriptorSetLayout;
    VkDescriptorSet       mAOUsageDescriptorSet;

    // Drawables:
    using enum Vertex::AttributeType;

    static constexpr VkIndexType IndexType = VK_INDEX_TYPE_UINT32;

    GeometryLayout mGeometryLayout{
        .VertexLayout = {Vec3, Vec2, Vec3, Vec4},
        .IndexType    = IndexType,
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
    float                 mInternalResolutionScale = 1.0f;
    VkSampleCountFlagBits mMultisample             = VK_SAMPLE_COUNT_1_BIT;
    bool                  mEnablePrepass           = true;
    bool                  mEnableAO                = false;

    // Dynamic uniform data including camera/lighting and more renderer settings:
    struct UBOData {
        glm::mat4 CameraViewProjection;
        glm::mat4 LightViewProjection;
        glm::vec3 ViewPos;
        float     DirectionalFactor = 3.0f;
        float     EnvironmentFactor = 0.05f;
        float     ShadowBiasMin     = 0.001f;
        float     ShadowBiasMax     = 0.003f;
        int       AOEnabled         = 0;
    } mUBOData;

    DynamicUniformBuffer mDynamicUBO;

    // Cubemap generation and background drawing:
    EnvironmentHandler mEnvHandler;
    // Shadowmap generation:
    ShadowmapHandler mShadowmapHandler;
    AABB             mSceneAABB;

    // Deletion queues:
    DeletionQueue mSceneDeletionQueue;
    DeletionQueue mMaterialDeletionQueue;
};
