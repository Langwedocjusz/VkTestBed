#pragma once

#include "GeometryProvider.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "VertexLayout.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

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

    void LoadScene(Scene &scene) override;

  private:
    void LoadProviders(Scene &scene);
    void LoadTextures(Scene &scene);
    void LoadInstances(Scene &scene);

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
        size_t VertexCount;

        Buffer IndexBuffer;
        size_t IndexCount;

        std::vector<glm::mat4> Transforms;
        size_t TextureId = 0;
    };

    std::vector<Drawable> mColoredDrawables;
    std::vector<Drawable> mTexturedDrawables;

    std::map<size_t, size_t> mColoredProviderMap;
    std::map<size_t, size_t> mTexturedProviderMap;

    VkDescriptorSetLayout mTextureDescriptorSetLayout;
    VkDescriptorPool mTextureDescriptorPool;

    struct Texture {
        Image TexImage;
        VkImageView View;
        VkDescriptorSet DescriptorSet;
    };

    std::vector<Texture> mTextures;
    VkSampler mSampler;
};