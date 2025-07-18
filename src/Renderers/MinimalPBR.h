#pragma once

#include "DeletionQueue.h"
#include "Descriptor.h"
#include "EnvironmentHandler.h"
#include "GeometryData.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Scene.h"
#include "Texture.h"

class MinimalPbrRenderer final : public IRenderer {
  public:
    MinimalPbrRenderer(VulkanContext &ctx, FrameInfo &info,
                       std::unique_ptr<Camera> &camera);
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

    struct Drawable;
    void CreateBuffers(VkCommandPool pool, Drawable &drawable, GeometryData &geo);

  private:
    // Framebuffer related things:
    const float mInternalResolutionScale = 1.0f;
    const VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;

    const VkFormat mDepthFormat = VK_FORMAT_D32_SFLOAT;
    Image mDepthBuffer;
    VkImageView mDepthBufferView;

    // Common resources:
    VkSampler mSampler2D;

    // Main material pass:
    Pipeline mMainPipeline;

    using enum Vertex::AttributeType;

    GeometryLayout mGeometryLayout{
        .VertexLayout = {Vec3, Vec2, Vec3, Vec4},
        .IndexType = VK_INDEX_TYPE_UINT32,
    };

    struct MaterialPCData {
        glm::vec3 ViewPos;
        float AlphaCutoff;
        glm::mat4 Transform;
        int DoubleSided;
    };

    Texture mDefaultAlbedo;
    Texture mDefaultRoughness;
    Texture mDefaultNormal;

    std::map<SceneKey, Texture> mImages;

    VkDescriptorSetLayout mMaterialDescriptorSetLayout;
    DescriptorAllocator mMaterialDescriptorAllocator;

    struct Material {
        VkDescriptorSet DescriptorSet;
        float AlphaCutoff = 0.5f;
        bool DoubleSided = false;
    };

    std::map<SceneKey, Material> mMaterials;

    struct Drawable {
        Buffer VertexBuffer;
        uint32_t VertexCount;

        Buffer IndexBuffer;
        uint32_t IndexCount;

        SceneKey MaterialKey = 0;

        std::vector<glm::mat4> Instances;
    };

    // Drawables correspond to mesh primitives
    // so DrawableKey is the pair (MeshKey, PrimitiveId)
    using DrawableKey = std::pair<SceneKey, size_t>;
    std::map<DrawableKey, Drawable> mDrawables;

    std::vector<DrawableKey> mSingleSidedDrawableKeys;
    std::vector<DrawableKey> mDoubleSidedDrawableKeys;

    // Cubemap generation and background drawing:
    EnvironmentHandler mEnvHandler;

    Pipeline mBackgroundPipeline;

    struct BackgroundPCData {
        glm::vec4 TopLeft;
        glm::vec4 TopRight;
        glm::vec4 BottomLeft;
        glm::vec4 BottomRight;
    };

    // Deletion queues:
    DeletionQueue mSceneDeletionQueue;
    DeletionQueue mMaterialDeletionQueue;
};