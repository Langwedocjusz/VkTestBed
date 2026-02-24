#pragma once

#include "Descriptor.h"
#include "DynamicUniformBuffer.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Scene.h"
#include "Texture.h"
#include "VertexLayout.h"

class Minimal3DRenderer final : public IRenderer {
  public:
    Minimal3DRenderer(VulkanContext &ctx, FrameInfo &info, Camera &camera);
    ~Minimal3DRenderer();

    void OnUpdate([[maybe_unused]] float deltaTime) override;
    void OnImGui() override;
    void OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj) override;

    void RecreateSwapchainResources() override;
    void RebuildPipelines() override;
    void LoadScene(const Scene &scene) override;
    void RenderObjectId(VkCommandBuffer cmd, float x, float y) override;

  private:
    void LoadMeshes(const Scene &scene);
    void LoadImages(const Scene &scene);
    void LoadMaterials(const Scene &scene);
    void LoadMeshMaterials(const Scene &scene);
    void LoadObjects(const Scene &scene);

  private:
    const float    mInternalResolutionScale = 1.0f;
    const VkFormat mRenderTargetFormat      = VK_FORMAT_R8G8B8A8_SRGB;

    const VkFormat mDepthFormat = VK_FORMAT_D32_SFLOAT;
    Texture        mDepthBuffer;

    Pipeline mColoredPipeline;
    Pipeline mTexturedPipeline;

    GeometryLayout mColoredLayout{
        .VertexLayout = Vertex::Layout{.HasNormal = true, .HasColor = true},
        .IndexType    = VK_INDEX_TYPE_UINT32,
    };

    GeometryLayout mTexturedLayout{
        .VertexLayout = Vertex::Layout{.HasTexCoord = true, .HasNormal = true},
        .IndexType    = VK_INDEX_TYPE_UINT32,
    };

    struct Drawable {
        Buffer   VertexBuffer;
        uint32_t VertexCount;

        Buffer   IndexBuffer;
        uint32_t IndexCount;

        SceneKey Material;

        std::vector<glm::mat4> Instances;
    };

    using DrawableKey = std::pair<SceneKey, size_t>;

    std::map<DrawableKey, Drawable> mColoredDrawables;
    std::map<DrawableKey, Drawable> mTexturedDrawables;

    Texture                     mDefaultImage;
    std::map<SceneKey, Texture> mImages;

    VkDescriptorSetLayout      mTextureDescriptorSetLayout;
    DynamicDescriptorAllocator mTextureDescriptorAllocator;

    struct Material {
        float           AlphaCutoff = 0.5f;
        VkDescriptorSet DescriptorSet;
    };

    std::map<SceneKey, Material> mMaterials;

    // For textured pipeline to also upload alpha cutoff:
    struct PushConstantData {
        // Vec4 because of std430 alignment rules:
        glm::vec4 AlphaCutoff;
        glm::mat4 Transform;
    };

    VkSampler mSampler;

    // Camera and light projection handling:
    struct UBOData {
        glm::mat4 CameraViewProjection = glm::mat4(1.0f);
    } mUBOData;

    DynamicUniformBuffer mDynamicUBO;

    DeletionQueue mSceneDeletionQueue;
};