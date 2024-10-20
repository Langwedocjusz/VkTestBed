#pragma once

#include "Image.h"
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
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void DrawScene(VkCommandBuffer commandBuffer);
    void DrawUI(VkCommandBuffer commandBuffer, uint32_t imageIndex);

  private:
    Pipeline mGraphicsPipeline;

    Image mRenderTarget;
    VkImageView mRenderTargetView;
    const VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;
    const float mInternalResolutionScale = 1.0f;

    std::vector<VkCommandBuffer> mCommandBuffers;
};