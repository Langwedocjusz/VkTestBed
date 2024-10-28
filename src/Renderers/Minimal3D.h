#pragma once

#include "Buffer.h"
#include "Pipeline.h"
#include "Renderer.h"
#include <vulkan/vulkan_core.h>

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
    void LoadInstances(Scene &scene);

  private:
    const float mInternalResolutionScale = 1.0f;
    const VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;

    const VkFormat mDepthFormat = VK_FORMAT_D32_SFLOAT;
    Image mDepthBuffer;
    VkImageView mDepthBufferView;

    Pipeline mGraphicsPipeline;

    struct Drawable {
        Buffer VertexBuffer;
        size_t VertexCount;

        Buffer IndexBuffer;
        size_t IndexCount;

        std::vector<glm::mat4> Transforms;
    };

    std::vector<Drawable> mDrawables;
};