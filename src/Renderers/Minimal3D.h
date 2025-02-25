#pragma once

#include "Descriptor.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Scene.h"

#include <vulkan/vulkan.h>

#include <map>

class Minimal3DRenderer : public IRenderer {
  public:
    Minimal3DRenderer(VulkanContext &ctx, FrameInfo &info, RenderContext::Queues &queues,
                      std::unique_ptr<Camera> &camera);
    ~Minimal3DRenderer();

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

  private:
    const float mInternalResolutionScale = 1.0f;
    const VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;

    const VkFormat mDepthFormat = VK_FORMAT_D32_SFLOAT;
    Image mDepthBuffer;
    VkImageView mDepthBufferView;

    Pipeline mColoredPipeline;
    Pipeline mTexturedPipeline;

    using enum Vertex::AttributeType;

    GeometryLayout mColoredLayout{
        .VertexLayout = {Vec3, Vec3, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT32,
    };

    GeometryLayout mTexturedLayout{
        .VertexLayout = {Vec3, Vec2, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT32,
    };

    struct Drawable {
        Buffer VertexBuffer;
        uint32_t VertexCount;

        Buffer IndexBuffer;
        uint32_t IndexCount;

        SceneKey Material;
    };

    struct Mesh {
        std::vector<Drawable> Drawables;
        std::vector<glm::mat4> Transforms;
    };

    std::map<SceneKey, Mesh> mColoredMeshes;
    std::map<SceneKey, Mesh> mTexturedMeshes;

    struct Texture {
        Image TexImage;
        VkImageView View;
    };

    Texture mDefaultImage;
    std::map<SceneKey, Texture> mImages;

    VkDescriptorSetLayout mTextureDescriptorSetLayout;
    DescriptorAllocator mTextureDescriptorAllocator;

    struct Material {
        float AlphaCutoff = 0.5f;
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

    DeletionQueue mSceneDeletionQueue;
};