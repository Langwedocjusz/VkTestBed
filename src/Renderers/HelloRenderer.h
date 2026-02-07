#pragma once

#include "DynamicUniformBuffer.h"
#include "Pipeline.h"
#include "Renderer.h"

#include "volk.h"

#include <map>

class HelloRenderer final : public IRenderer {
  public:
    HelloRenderer(VulkanContext &ctx, FrameInfo &info, Camera &camera);
    ~HelloRenderer();

    void OnUpdate([[maybe_unused]] float deltaTime) override;
    void OnImGui() override;
    void OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj) override;

    void RecreateSwapchainResources() override;
    void RebuildPipelines() override;
    void LoadScene(const Scene &scene) override;
    void RenderObjectId(VkCommandBuffer cmd, float x, float y) override;

  private:
    void LoadMeshes(const Scene &scene);
    void LoadObjects(const Scene &scene);

  private:
    const float    mInternalResolutionScale = 1.0f;
    const VkFormat mRenderTargetFormat      = VK_FORMAT_R8G8B8A8_SRGB;

    Pipeline mGraphicsPipeline;

    GeometryLayout mGeometryLayout{
        .VertexLayout = {Vertex::AttributeType::Vec3, Vertex::AttributeType::Vec3},
        .IndexType    = VK_INDEX_TYPE_UINT16,
    };

    struct Drawable {
        Buffer   VertexBuffer;
        uint32_t VertexCount;

        Buffer   IndexBuffer;
        uint32_t IndexCount;

        SceneKey Instances;
    };

    using DrawableKey = std::pair<SceneKey, size_t>;

    std::map<DrawableKey, Drawable> mDrawables;

    struct InstanceData {
        glm::mat4 Transform;
    };

    std::map<SceneKey, std::vector<InstanceData>> mInstanceData;

    struct UBOData {
        glm::mat4 CameraViewProjection = glm::mat4(1.0f);
    } mUBOData;

    DynamicUniformBuffer mDynamicUBO;

    DeletionQueue mSceneDeletionQueue;
};