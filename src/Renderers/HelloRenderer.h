#pragma once

#include "GeometryProvider.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "VertexLayout.h"

#include <vulkan/vulkan.h>

#include <map>

class HelloRenderer : public IRenderer {
  public:
    HelloRenderer(VulkanContext &ctx, FrameInfo &info, RenderContext::Queues &queues,
                  std::unique_ptr<Camera> &camera);
    ~HelloRenderer();

    void OnUpdate([[maybe_unused]] float deltaTime) override;
    void OnImGui() override;
    void OnRender() override;

    void CreateSwapchainResources() override;
    void RebuildPipelines() override;
    void LoadScene(Scene &scene) override;

  private:
    void LoadProviders(Scene &scene);
    void LoadInstances(Scene &scene);

  private:
    const float mInternalResolutionScale = 1.0f;
    const VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;

    Pipeline mGraphicsPipeline;

    GeometryLayout mGeometryLayout{
        .VertexLayout = {Vertex::AttributeType::Vec3, Vertex::AttributeType::Vec3},
        .IndexType = VK_INDEX_TYPE_UINT16,
    };

    struct Drawable {
        Buffer VertexBuffer;
        uint32_t VertexCount;

        Buffer IndexBuffer;
        uint32_t IndexCount;
    };

    struct Mesh {
        std::vector<Drawable> Drawables;
        std::vector<glm::mat4> Transforms;
    };

    std::map<size_t, size_t> mMeshIdMap;

    std::vector<Mesh> mMeshes;

    DeletionQueue mSceneDeletionQueue;
};