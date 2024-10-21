#pragma once

#include "Pipeline.h"
#include "Renderer.h"

class HelloRenderer : public IRenderer {
  public:
    HelloRenderer(VulkanContext &ctx, FrameInfo &info, RenderContext::Queues &queues);
    ~HelloRenderer();

    void OnUpdate([[maybe_unused]] float deltaTime) override;
    void OnImGui() override;
    void OnRender() override;

    void CreateSwapchainResources() override;

    void LoadScene(const Scene &scene) override;

  private:
    Pipeline mGraphicsPipeline;

    const float mInternalResolutionScale = 1.0f;
    const VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;
};