#pragma once

#include "Buffer.h"
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

    void LoadScene(Scene &scene) override;

  private:
    const float mInternalResolutionScale = 1.0f;
    const VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;

    VkDescriptorSetLayout mDescriptorSetLayout;
    VkDescriptorPool mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;

    Pipeline mGraphicsPipeline;

    struct Drawable {
        Buffer VertexBuffer;
        size_t VertexCount;

        Buffer IndexBuffer;
        size_t IndexCount;

        std::vector<glm::mat4> Transforms;
    };

    std::vector<Drawable> mDrawables;

    struct UniformBufferObject {
        glm::mat4 ViewProjection = glm::mat4(1.0f);
    };
    UniformBufferObject mUBOData;

    std::vector<Buffer> mUniformBuffers;
};