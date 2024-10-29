#pragma once

#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Pipeline.h"
#include "Renderer.h"

class HelloRenderer : public IRenderer {
  public:
    HelloRenderer(VulkanContext &ctx, FrameInfo &info, RenderContext::Queues &queues,
                  std::unique_ptr<Camera> &camera);
    ~HelloRenderer();

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

    Pipeline mGraphicsPipeline;

    struct Drawable {
        VertexBuffer Vert;
        IndexBuffer Idx;

        std::vector<glm::mat4> Transforms;
    };

    std::vector<Drawable> mDrawables;
};