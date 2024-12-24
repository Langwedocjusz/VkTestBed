#pragma once

#include "DeletionQueue.h"
#include "Descriptor.h"
#include "GeometryProvider.h"
#include "ImageLoaders.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "VertexLayout.h"

#include <vulkan/vulkan.h>

#include <map>
#include <vulkan/vulkan_core.h>

class MinimalPbrRenderer : public IRenderer {
  public:
    MinimalPbrRenderer(VulkanContext &ctx, FrameInfo &info, RenderContext::Queues &queues,
                       std::unique_ptr<Camera> &camera);
    ~MinimalPbrRenderer();

    void OnUpdate([[maybe_unused]] float deltaTime) override;
    void OnImGui() override;
    void OnRender() override;

    void CreateSwapchainResources() override;
    void RebuildPipelines() override;
    void LoadScene(Scene &scene) override;

  private:
    void LoadProviders(Scene &scene);
    void LoadTextures(Scene &scene);
    void LoadMeshMaterials(Scene &scene);
    void LoadInstances(Scene &scene);

    void TextureFromPath(Image &img, VkImageView &view, ::Material::ImageSource *source);
    void TextureFromPath(Image &img, VkImageView &view, ::Material::ImageSource *source,
                         ::ImageLoaders::Image2DData &defaultData, bool unorm = false);

  private:
    const float mInternalResolutionScale = 1.0f;
    const VkFormat mRenderTargetFormat = VK_FORMAT_R8G8B8A8_SRGB;

    const VkFormat mDepthFormat = VK_FORMAT_D32_SFLOAT;
    Image mDepthBuffer;
    VkImageView mDepthBufferView;

    Pipeline mPipeline;

    using enum Vertex::AttributeType;

    GeometryLayout mGeometryLayout{
        .VertexLayout = {Vec3, Vec2, Vec3, Vec4},
        .IndexType = VK_INDEX_TYPE_UINT32,
    };

    struct Drawable {
        Buffer VertexBuffer;
        uint32_t VertexCount;

        Buffer IndexBuffer;
        uint32_t IndexCount;

        size_t MaterialId = 0;
    };

    struct Mesh {
        std::vector<Drawable> Drawables;
        std::vector<glm::mat4> Transforms;
    };

    std::vector<Mesh> mMeshes;

    std::map<size_t, size_t> mIdMap;

    VkDescriptorSetLayout mTextureDescriptorSetLayout;
    DescriptorAllocator mTextureDescriptorAllocator;

    struct Material {
        VkDescriptorSet DescriptorSet;

        Image AlbedoImage;
        VkImageView AlbedoView = VK_NULL_HANDLE;

        Image RoughnessImage;
        VkImageView RoughnessView = VK_NULL_HANDLE;

        Image NormalImage;
        VkImageView NormalView = VK_NULL_HANDLE;

        float AlphaCutoff = 0.5f;
    };

    std::vector<Material> mMaterials;

    // For textured pipeline to also upload alpha cutoff:
    struct PushConstantData {
        // Vec4 because of std430 alignment rules:
        glm::vec4 AlphaCutoff;
        glm::mat4 Transform;
    };

    VkSampler mAlbedoSampler;
    VkSampler mRoughnessSampler;
    VkSampler mNormalSampler;

    DeletionQueue mMaterialDeletionQueue;
};