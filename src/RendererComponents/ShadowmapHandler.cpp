#include "ShadowmapHandler.h"
#include "Pch.h"

#include "Barrier.h"
#include "BufferUtils.h"
#include "Common.h"
#include "Descriptor.h"
#include "ImageUtils.h"
#include "Pipeline.h"
#include "Sampler.h"
#include "VertexLayout.h"

#include "imgui.h"
#include <imgui_impl_vulkan.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <limits>

ShadowmapHandler::ShadowmapHandler(VulkanContext &ctx, VkFormat debugColorFormat,
                                   VkFormat debugDepthFormat)
    : mCtx(ctx), mDebugColorFormat(debugColorFormat), mDebugDepthFormat(debugDepthFormat),
      mMainDeletionQueue(ctx), mPipelineDeletionQueue(ctx)
{
    // Create the shadowmap:
    Image2DInfo shadowmapInfo{
        .Extent = {ShadowmapResolution, ShadowmapResolution},
        .Format = ShadowmapFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = 1,
        .Layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    mShadowmap = MakeTexture::Texture2D(mCtx, "Shadowmap", shadowmapInfo);
    mMainDeletionQueue.push_back(mShadowmap);

    // Create a sampler for the shadowmap:
    mSampler = SamplerBuilder("MinimalPbrSamplerShadowmap")
                   .SetMagFilter(VK_FILTER_LINEAR)
                   .SetMinFilter(VK_FILTER_LINEAR)
                   .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
                   .SetBorderColor(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE)
                   .SetCompareOp(VK_COMPARE_OP_LESS)
                   .Build(mCtx, mMainDeletionQueue);

    // Set up a descriptor for sampling the shadow map:
    {
        // Create static descriptor pool:
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        };

        mStaticDescriptorPool = Descriptor::InitPool(mCtx, 1, poolCounts);
        mMainDeletionQueue.push_back(mStaticDescriptorPool);

        // Create descriptor set layout for sampling the shadowmap
        // and allocate the corresponding descriptor set:
        mShadowmapDescriptorSetLayout =
            DescriptorSetLayoutBuilder("MinimalPBRShadowmapDescriptorLayout")
                .AddCombinedSampler(0, VK_SHADER_STAGE_FRAGMENT_BIT)
                .Build(ctx, mMainDeletionQueue);

        mShadowmapDescriptorSet = Descriptor::Allocate(mCtx, mStaticDescriptorPool,
                                                       mShadowmapDescriptorSetLayout);

        // Update the shadowmap descriptor:
        DescriptorUpdater(mShadowmapDescriptorSet)
            .WriteCombinedSampler(0, mShadowmap.View, mSampler)
            .Update(mCtx);
    }

    // Setup debug view of the shadowmap in imgui:
    {
        // For debug view in imgui:
        mDebugSampler = SamplerBuilder("MinimalPbrSampler2D")
                            .SetMagFilter(VK_FILTER_LINEAR)
                            .SetMinFilter(VK_FILTER_LINEAR)
                            .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                            .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                            .SetMaxLod(12.0f)
                            .Build(mCtx, mMainDeletionQueue);

        mDebugTextureDescriptorSet = ImGui_ImplVulkan_AddTexture(
            mDebugSampler, mShadowmap.View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // Create Vertex Buffer for debug vizualiztion:
    {
        // This way of making vertex buffer dynamic results in some tearing artefacts
        // on camera movement. Perfect solution would be to use 3 per-frame buffers.
        // But the artefacts are not that bad and this is for debug only.

        VkDeviceSize vertexBufferSize =
            mVertexBufferData.size() * sizeof(mVertexBufferData[0]);

        auto bufUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        auto bufFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT;

        mDebugFrustumVertexBuffer =
            Buffer::Create(mCtx, "ShadowmapDebugFrustumVertexBuffer", vertexBufferSize,
                           bufUsage, bufFlags);
        mMainDeletionQueue.push_back(mDebugFrustumVertexBuffer);
    }

    // Create Index Buffer for debug visualization:
    {
        // Index buffer doesn't need to be dynamic:
        OpaqueBuffer indexData(NumIdxPerFrustum, NumIdxPerFrustum * sizeof(uint16_t),
                               alignof(uint16_t));

        // clang-format off
        new (indexData.Data) uint16_t[NumIdxPerFrustum]{
            // Near
            0, 1, 2, 1, 3, 2,
            // Far
            6, 5, 4, 6, 7, 5,
            // Left
            0, 6, 4, 0, 2, 6,
            // Right
            1, 5, 7, 1, 7, 3,
            // Top
            0, 4, 5, 0, 5, 1,
            // Bottom
            2, 7, 6, 2, 3, 7,
        };
        // clang-format on

        mDebugFrustumIndexBuffer =
            MakeBuffer::Index(mCtx, "ShadowmapDebugFrustumVertexBuffer", indexData);
        mMainDeletionQueue.push_back(mDebugFrustumIndexBuffer);
    }
}

ShadowmapHandler::~ShadowmapHandler()
{
    mPipelineDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void ShadowmapHandler::RebuildPipelines(const Vertex::Layout &vertexLayout,
                                        VkDescriptorSetLayout materialDSLayout,
                                        VkSampleCountFlagBits debugMultisampling)
{
    mPipelineDeletionQueue.flush();

    mOpaquePipeline =
        PipelineBuilder("ShadowmapPipeline")
            .SetShaderPathVertex("assets/spirv/shadows/ShadowmapAlphaVert.spv")
            .SetVertexInput(vertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
            .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
            .SetPushConstantSize(sizeof(mShadowPCData))
            .EnableDepthTest()
            .SetDepthFormat(ShadowmapFormat)
            .Build(mCtx, mPipelineDeletionQueue);

    mAlphaPipeline =
        PipelineBuilder("ShadowmapPipeline")
            .SetShaderPathVertex("assets/spirv/shadows/ShadowmapAlphaVert.spv")
            .SetShaderPathFragment("assets/spirv/shadows/ShadowmapAlphaFrag.spv")
            .SetVertexInput(vertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
            .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
            .SetPushConstantSize(sizeof(mShadowPCData))
            .AddDescriptorSetLayout(materialDSLayout)
            .EnableDepthTest()
            .SetDepthFormat(ShadowmapFormat)
            .Build(mCtx, mPipelineDeletionQueue);

    mDebugPipeline =
        PipelineBuilder("ShadowmapDebugPipeline")
            .SetShaderPathVertex("assets/spirv/shadows/ShadowDebugVert.spv")
            .SetShaderPathFragment("assets/spirv/shadows/ShadowDebugFrag.spv")
            .SetVertexInput(mDebugGeometryLayout.VertexLayout, 0,
                            VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
            .SetPushConstantSize(sizeof(mDebugPCData))
            .SetColorFormat(mDebugColorFormat)
            .EnableDepthTest()
            .SetDepthFormat(mDebugDepthFormat)
            .SetStencilFormat(mDebugDepthFormat)
            .SetMultisampling(debugMultisampling)
            .EnableBlending()
            .Build(mCtx, mPipelineDeletionQueue);
}

void ShadowmapHandler::OnUpdate(Frustum camFr, glm::vec3 lightDir, AABB sceneAABB)
{
    // Construct worldspace coords for the shadow sampling part of the frustum:
    auto ScaleFarVector = [this](glm::vec4 near, glm::vec4 &far) {
        auto normalized = glm::normalize(glm::vec3(far - near));
        far             = near + mShadowDist * glm::vec4(normalized, 0.0f);
    };

    if (!mFreezeFrustum)
    {
        mShadowFrustum = camFr;
        ScaleFarVector(mShadowFrustum.NearTopLeft, mShadowFrustum.FarTopLeft);
        ScaleFarVector(mShadowFrustum.NearTopRight, mShadowFrustum.FarTopRight);
        ScaleFarVector(mShadowFrustum.NearBottomLeft, mShadowFrustum.FarBottomLeft);
        ScaleFarVector(mShadowFrustum.NearBottomRight, mShadowFrustum.FarBottomRight);

        auto frustumVerts = mShadowFrustum.GetVertices();

        std::copy(&frustumVerts[0], &frustumVerts[8], &mVertexBufferData[0]);
    }

    // Construct light view matrix, looking along the light direction:
    // Up vector doesn't really matter.
    glm::mat4 lightView = glm::lookAt(lightDir, glm::vec3(0.0f), glm::vec3(0, -1, 0));

    // Transform the shadowed frustum to light view space:
    auto frustumVertices = mShadowFrustum.GetVertices();

    for (auto &vert : frustumVertices)
    {
        // Don't need to multiply y component by -1
        // since camera already handles that in its
        // frusutm generation code.
        vert = lightView * vert;
    }

    // Find the extents of the frustum in light view space to get ortho projection bounds:
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();

    for (auto &vert : frustumVertices)
    {
        minX = std::min(minX, vert.x);
        minY = std::min(minY, vert.y);
        minZ = std::min(minZ, vert.z);

        maxX = std::max(maxX, vert.x);
        maxY = std::max(maxY, vert.y);
        maxZ = std::max(maxZ, vert.z);
    }

    // Clip XY positions to texel size in world space to avoid shimmering artefact:
    float texelSizeX = (maxX - minX) / ShadowmapResolution;
    float texelSizeY = (maxY - minY) / ShadowmapResolution;

    minX = std::floor(minX / texelSizeX) * texelSizeX;
    maxX = std::floor(maxX / texelSizeX) * texelSizeX;
    minY = std::floor(minY / texelSizeY) * texelSizeY;
    maxY = std::floor(maxY / texelSizeY) * texelSizeY;

    // Update minZ and maxZ values based on scene bounding box:
    if (mFitToScene)
    {
        auto verts = sceneAABB.GetVertices();

        for (auto &vert : verts)
        {
            // Multiplication by -1 needed here, since BBOX positions
            // don't take vulkans inverted y-axis into account:
            vert.y *= -1.0f;
            vert = glm::vec3(lightView * glm::vec4(vert, 1.0f));
        }

        auto minAABB = std::numeric_limits<float>::max();
        auto maxAABB = std::numeric_limits<float>::lowest();

        for (auto vert : verts)
        {
            minAABB = std::min(minAABB, vert.z);
            maxAABB = std::max(maxAABB, vert.z);
        }

        minZ = std::min(minZ, minAABB);
        maxZ = std::max(maxZ, maxAABB);
    }

    if (mDebugView)
    {
        auto inverseLightView = glm::inverse(lightView);

        mVertexBufferData[8]  = inverseLightView * glm::vec4(minX, maxY, minZ, 1.0f);
        mVertexBufferData[9]  = inverseLightView * glm::vec4(maxX, maxY, minZ, 1.0f);
        mVertexBufferData[10] = inverseLightView * glm::vec4(minX, minY, minZ, 1.0f);
        mVertexBufferData[11] = inverseLightView * glm::vec4(maxX, minY, minZ, 1.0f);
        mVertexBufferData[12] = inverseLightView * glm::vec4(minX, maxY, maxZ, 1.0f);
        mVertexBufferData[13] = inverseLightView * glm::vec4(maxX, maxY, maxZ, 1.0f);
        mVertexBufferData[14] = inverseLightView * glm::vec4(minX, minY, maxZ, 1.0f);
        mVertexBufferData[15] = inverseLightView * glm::vec4(maxX, minY, maxZ, 1.0f);

        Buffer::UploadToMapped(mDebugFrustumVertexBuffer, mVertexBufferData.data(),
                               mVertexBufferData.size() * sizeof(mVertexBufferData[0]));
    }

    // Construct the projection matrix:
    glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

    mLightViewProj = lightProj * lightView;
}

void ShadowmapHandler::OnImGui()
{
    ImGui::Checkbox("Debug View", &mDebugView);
    ImGui::Checkbox("Freeze Frustum", &mFreezeFrustum);
    ImGui::Checkbox("Fit to scene", &mFitToScene);

    if (!mFreezeFrustum)
        ImGui::SliderFloat("Shadow Dist", &mShadowDist, 1.0f, 40.0f);

    ImGui::Image((ImTextureID)mDebugTextureDescriptorSet, ImVec2(512, 512));
}

VkExtent2D ShadowmapHandler::GetExtent() const
{
    auto width  = mShadowmap.Img.Info.extent.width;
    auto height = mShadowmap.Img.Info.extent.height;

    return VkExtent2D{width, height};
}

void ShadowmapHandler::BeginShadowPass(VkCommandBuffer cmd)
{
    barrier::ImageBarrierDepthToRender(cmd, mShadowmap.Img.Handle);
    common::BeginRenderingDepth(cmd, GetExtent(), mShadowmap.View, false, true);
}

void ShadowmapHandler::BindOpaquePipeline(VkCommandBuffer cmd)
{
    mOpaquePipeline.Bind(cmd);
    common::ViewportScissor(cmd, GetExtent());
}

void ShadowmapHandler::BindAlphaPipeline(VkCommandBuffer cmd)
{
    mAlphaPipeline.Bind(cmd);
    common::ViewportScissor(cmd, GetExtent());
}

void ShadowmapHandler::EndShadowPass(VkCommandBuffer cmd)
{
    vkCmdEndRendering(cmd);
    barrier::ImageBarrierDepthToSample(cmd, mShadowmap.Img.Handle);
}

void ShadowmapHandler::PushConstantOpaque(VkCommandBuffer cmd, glm::mat4 transform)
{
    mShadowPCData.LightMVP = mLightViewProj * transform;

    vkCmdPushConstants(cmd, mOpaquePipeline.Layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                       sizeof(mShadowPCData), &mShadowPCData);
}

void ShadowmapHandler::PushConstantAlpha(VkCommandBuffer cmd, glm::mat4 transform)
{
    mShadowPCData.LightMVP = mLightViewProj * transform;

    vkCmdPushConstants(cmd, mAlphaPipeline.Layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                       sizeof(mShadowPCData), &mShadowPCData);
}

void ShadowmapHandler::BindAlphaMaterialDS(VkCommandBuffer cmd,
                                           VkDescriptorSet materialDS)
{
    mAlphaPipeline.BindDescriptorSet(cmd, materialDS, 0);
}

void ShadowmapHandler::DrawDebugShapes(VkCommandBuffer cmd, glm::mat4 viewProj,
                                       VkExtent2D extent)
{
    if (!mDebugView)
        return;

    // Bind pipeline:
    mDebugPipeline.Bind(cmd);
    common::ViewportScissor(cmd, extent);

    // Bind geometry buffers:
    VkBuffer     vertBuffer = mDebugFrustumVertexBuffer.Handle;
    VkDeviceSize vertOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

    vkCmdBindIndexBuffer(cmd, mDebugFrustumIndexBuffer.Handle, 0,
                         mDebugGeometryLayout.IndexType);

    // To update PC data:
    auto PushConstants = [this, cmd]() {
        vkCmdPushConstants(cmd, mDebugPipeline.Layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                           sizeof(mDebugPCData), &mDebugPCData);
    };

    mDebugPCData.ViewProj = viewProj;

    // Draw view frustum:
    mDebugPCData.Color = glm::vec4(0.2f, 0.2f, 0.6f, 0.5f);
    PushConstants();

    vkCmdDrawIndexed(cmd, NumIdxPerFrustum, 1, 0, 0, 0);

    // Draw shadow projection bounds:
    mDebugPCData.Color = glm::vec4(0.9f, 0.9f, 0.2f, 0.2f);
    PushConstants();

    vkCmdDrawIndexed(cmd, NumIdxPerFrustum, 1, 0, 8, 0);
}

// Work in progress - less conservative zmin/zmax calculation method:

/*static std::optional<glm::vec3> IntersectFace(std::array<glm::vec3, 2> lineSegment,
std::array<glm::vec3,4> face)
{
    //Construct ray from line segment:
    auto rayOrigin = lineSegment[0];
    auto rayEnd = lineSegment[1];

    auto rayDir = rayEnd - rayOrigin;
    auto tEnd = glm::length(rayDir);
    rayDir = glm::normalize(rayDir);

    //Construct plane from face:
    //Assumes all verts are actually coplanar.
    auto planeOrigin = face[0];

    auto planeNormal = glm::cross(face[3] - planeOrigin, face[1] - planeOrigin);
    planeNormal = glm::normalize(planeNormal);

    //Get intersection 'time':
    auto denom = glm::dot(rayDir, planeNormal);

    if (denom == 0.0f)
        return std::nullopt;

    auto t = - glm::dot(rayOrigin - planeOrigin, planeNormal) / denom;

    //Check if it is inside the segment:
    if (t < 0.0f || t > tEnd)
        return std::nullopt;

    //Reconstruct the intersection:
    auto intersection = rayOrigin + t * rayDir;

    //Check if it lies within the face:
    std::array diffVecs{
        intersection - face[0],
        intersection - face[1],
        intersection - face[2],
        intersection - face[3],
    };

    std::array testVecs{
        //Orientation is important:
        face[0] - face[1],
        face[1] - face[2],
        face[2] - face[3],
        face[3] - face[0],
    };

    for (size_t idx=0; idx<4; idx++)
    {
        if (glm::dot(planeNormal, glm::cross(diffVecs[idx], testVecs[idx])) > 0.0f)
            return std::nullopt;
    }

    //If all checks passed return intersection:
    return intersection;
}*/

/*
{
    //TODO: Intersection of the AABB with extruded frustum for less conservative Z
bounds

    float minAABB = std::numeric_limits<float>::max();
    float maxAABB = std::numeric_limits<float>::lowest();

    //Get bounding box vertices in lightspace:
    auto verts = sceneAABB.GetVertices();

    for (auto& vert : verts)
    {
        vert.z *= -1.0f;
        vert = glm::vec3(lightView * glm::vec4(vert, 1.0f));
    }

    //Compare z values with verts within frustum projection XY extent:
    for (auto vert : verts)
    {
        bool ok_x = (minX < vert.x && vert.x < maxX);
        bool ok_y = (minY < vert.y && vert.y < maxY);

        if (ok_x && ok_y)
        {
            minAABB = std::min(minAABB, vert.z);
            maxAABB = std::max(maxAABB, vert.z);
        }
    }

    //Find intersections of bbox edges with extended frustum proj faces:
    auto edgeIds = sceneAABB.GetEdgesIds();

    std::array<std::array<glm::vec3, 4>, 6> faces;

    {
        //Cube vertices:
        std::array cv{
            glm::vec3(minX, minY, minZ),
            glm::vec3(maxX, minY, minZ),
            glm::vec3(maxX, maxY, minZ),
            glm::vec3(minX, maxY, minZ),
            glm::vec3(minX, minY, maxZ),
            glm::vec3(maxX, minY, maxZ),
            glm::vec3(maxX, maxY, maxZ),
            glm::vec3(minX, maxY, maxZ),
        };

        faces = {{
            {cv[0], cv[1], cv[2], cv[3]}, //Bottom
            {cv[4], cv[5], cv[6], cv[7]}, //Top
            {cv[0], cv[1], cv[5], cv[4]}, //Front
            {cv[2], cv[3], cv[7], cv[6]}, //Back
            {cv[3], cv[0], cv[4], cv[7]}, //Left
            {cv[1], cv[2], cv[6], cv[5]}, //Right
        }};
    }

    for (auto edgeId : edgeIds)
    {
        std::array<glm::vec3, 2> edge{
            verts[edgeId[0]],
            verts[edgeId[1]],
        };

        for (auto face : faces)
        {
            auto intersection = IntersectFace(edge, face);

            if (intersection.has_value())
            {
                minAABB = std::min(minAABB, intersection->z);
                maxAABB = std::max(maxAABB, intersection->z);
            }
        }
    }

    minZ = std::min(minZ, minAABB);
    maxZ = std::max(maxZ, maxAABB);
}*/