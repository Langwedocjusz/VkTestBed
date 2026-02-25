#include "ShadowmapHandler.h"
#include "Pch.h"

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
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <limits>

ShadowmapHandler::ShadowmapHandler(VulkanContext &ctx, VkFormat debugColorFormat,
                                   VkFormat debugDepthFormat)
    : mCtx(ctx), mDebugColorFormat(debugColorFormat), mDebugDepthFormat(debugDepthFormat),
      mMainDeletionQueue(ctx), mPipelineDeletionQueue(ctx)
{
    // Create the shadowmap base texture:
    Image2DInfo shadowmapInfo{
        .Extent = {ShadowmapResolution, ShadowmapResolution},
        .Format = ShadowmapFormat,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    mShadowmap =
        MakeTexture::Texture2DArray(mCtx, "Shadowmap", shadowmapInfo, NumCascades);
    mMainDeletionQueue.push_back(mShadowmap);

    // Create per-level views for rendering:
    for (uint32_t i = 0; i < NumCascades; i++)
    {
        std::string name        = "ShadowmapView" + std::to_string(i);
        auto        format      = shadowmapInfo.Format;
        auto        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

        mCascadeViews[i] = MakeView::ViewArraySingleLayer(mCtx, name, mShadowmap.Img,
                                                          format, aspectFlags, i);
    }

    for (auto &view : mCascadeViews)
        mMainDeletionQueue.push_back(view);

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

        // TODO: for no only preview first cascade
        mDebugTextureDescriptorSet = ImGui_ImplVulkan_AddTexture(
            mDebugSampler, mCascadeViews[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
    (void)vertexLayout; //TODO: Remove this

    mPipelineDeletionQueue.flush();

    mOpaquePipeline =
        PipelineBuilder("ShadowmapPipeline")
            .SetShaderPathVertex("assets/spirv/shadows/ShadowmapAlphaVert.spv")
            //TODO: restore this code in separate path:
            //.SetVertexInput(vertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
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
            //TODO: restore this code in separate path:
            //.SetVertexInput(vertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
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

Frustum ShadowmapHandler::ScaleCameraFrustum(Frustum camFrustum, float distNear,
                                             float distFar) const
{
    auto ScaleVecs = [=](glm::vec4 nearVec, glm::vec4 farVec) {
        auto diff3 = glm::normalize(glm::vec3(farVec - nearVec));
        auto diff4 = glm::vec4(diff3, 0.0f);

        auto newNear = nearVec + distNear * diff4;
        auto newFar  = nearVec + distFar * diff4;

        return std::pair{newNear, newFar};
    };

    auto [NearBottomLeft, FarBottomLeft] =
        ScaleVecs(camFrustum.NearBottomLeft, camFrustum.FarBottomLeft);

    auto [NearBottomRight, FarBottomRight] =
        ScaleVecs(camFrustum.NearBottomRight, camFrustum.FarBottomRight);

    auto [NearTopLeft, FarTopLeft] =
        ScaleVecs(camFrustum.NearTopLeft, camFrustum.FarTopLeft);

    auto [NearTopRight, FarTopRight] =
        ScaleVecs(camFrustum.NearTopRight, camFrustum.FarTopRight);

    return Frustum{
        .NearTopLeft     = NearTopLeft,
        .NearTopRight    = NearTopRight,
        .NearBottomLeft  = NearBottomLeft,
        .NearBottomRight = NearBottomRight,
        .FarTopLeft      = FarTopLeft,
        .FarTopRight     = FarTopRight,
        .FarBottomLeft   = FarBottomLeft,
        .FarBottomRight  = FarBottomRight,
    };
}

ShadowVolume ShadowmapHandler::GetBoundingVolume(Frustum   shadowFrustum,
                                                 glm::mat4 lightView) const
{
    // Transform the shadow frustum vertices to light view space:
    auto frustumVertices = shadowFrustum.GetVertices();

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

    return ShadowVolume{
        .MinX = minX,
        .MaxX = maxX,
        .MinY = minY,
        .MaxY = maxY,
        .MinZ = minZ,
        .MaxZ = maxZ,
    };
}

ShadowVolume ShadowmapHandler::FitVolumeToScene(ShadowVolume volume, AABB sceneAABB,
                                                glm::mat4 lightView) const
{
    // Update minZ and maxZ values based on scene bounding box:
    if (mFitToScene)
    {
        // Transform AABB vers to light-view space:
        auto verts = sceneAABB.GetVertices();

        for (auto &vert : verts)
        {
            // Multiplication by -1 needed here, since BBOX positions
            // don't take vulkans inverted y-axis into account:
            vert.y *= -1.0f;
            vert = glm::vec3(lightView * glm::vec4(vert, 1.0f));
        }

        // Take min-max of their positions to update volume values.
        // This is overly conservative as doing min-max of the
        // intersection between sceneAABB and volume would be sufficient.
        auto minAABB = std::numeric_limits<float>::max();
        auto maxAABB = std::numeric_limits<float>::lowest();

        for (auto vert : verts)
        {
            minAABB = std::min(minAABB, vert.z);
            maxAABB = std::max(maxAABB, vert.z);
        }

        volume.MinZ = std::min(volume.MinZ, minAABB);
        volume.MaxZ = std::max(volume.MaxZ, maxAABB);
    }

    return volume;
}

void ShadowmapHandler::OnUpdate(Frustum camFr, glm::vec3 frontDir, glm::vec3 lightDir,
                                AABB sceneAABB)
{
    // Construct light view matrix, looking along the light direction:
    // Up vector doesn't really matter - it changes rotation of the resulting
    // shadowmap about the light dir - which is mostly irrelevant,
    // as sampling coords will change covariantly.
    glm::mat4 lightView = glm::lookAt(lightDir, glm::vec3(0.0f), glm::vec3(0, -1, 0));

    // Make each cascade twice as big as the last:
    // TODO: Hardcodes number of cascades:
    const std::array<float, 4> distances{0.0f, mShadowDist, 3.0f * mShadowDist,
                                         7.0f * mShadowDist};

    // Calculate max bounds for use in shaders:
    {
        const auto  diff3      = glm::vec3(camFr.FarTopRight - camFr.NearTopRight);
        const float projFactor = glm::dot(frontDir, glm::normalize(diff3));

        for (size_t i = 0; i < NumCascades; i++)
            mBounds[i] = projFactor * distances[i + 1];
    }

    for (size_t idx = 0; idx < NumCascades; idx++)
    {
        auto &frustum       = mShadowFrustums[idx];
        auto &lightViewProj = mLightViewProjs[idx];

        // Construct worldspace coords for the shadow sampling part of the frustum:
        if (!mFreezeFrustum)
        {
            frustum = ScaleCameraFrustum(camFr, distances[idx], distances[idx + 1]);
        }

        // Construct light-aligned volume thightly bounding the shadowed frustum:
        ShadowVolume vol = GetBoundingVolume(frustum, lightView);

        // Extend the Z range of constructed volume to fit entire scene range:
        vol = FitVolumeToScene(vol, sceneAABB, lightView);

        // Construct the projection matrix:
        glm::mat4 lightProj =
            glm::ortho(vol.MinX, vol.MaxX, vol.MinY, vol.MaxY, vol.MinZ, vol.MaxZ);

        lightViewProj = lightProj * lightView;

        // Update debug view vertex data:
        // TODO: Only visualizes first cascade:
        if (mDebugView && idx == 0)
        {
            // Copy frustum verts
            auto frustumVerts = mShadowFrustums[idx].GetVertices();
            std::copy(&frustumVerts[0], &frustumVerts[8], &mVertexBufferData[0]);

            // Copy bounding volume verts, transformed back to world-space:
            auto invLightView = glm::inverse(lightView);

            mVertexBufferData[8] =
                invLightView * glm::vec4(vol.MinX, vol.MaxY, vol.MinZ, 1.0f);
            mVertexBufferData[9] =
                invLightView * glm::vec4(vol.MaxX, vol.MaxY, vol.MinZ, 1.0f);
            mVertexBufferData[10] =
                invLightView * glm::vec4(vol.MinX, vol.MinY, vol.MinZ, 1.0f);
            mVertexBufferData[11] =
                invLightView * glm::vec4(vol.MaxX, vol.MinY, vol.MinZ, 1.0f);
            mVertexBufferData[12] =
                invLightView * glm::vec4(vol.MinX, vol.MaxY, vol.MaxZ, 1.0f);
            mVertexBufferData[13] =
                invLightView * glm::vec4(vol.MaxX, vol.MaxY, vol.MaxZ, 1.0f);
            mVertexBufferData[14] =
                invLightView * glm::vec4(vol.MinX, vol.MinY, vol.MaxZ, 1.0f);
            mVertexBufferData[15] =
                invLightView * glm::vec4(vol.MaxX, vol.MinY, vol.MaxZ, 1.0f);

            // Update gpu-visible buffer:
            Buffer::UploadToMapped(mDebugFrustumVertexBuffer, mVertexBufferData.data(),
                                   mVertexBufferData.size() *
                                       sizeof(mVertexBufferData[0]));
        }
    }
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

void ShadowmapHandler::PushConstantOpaque(VkCommandBuffer cmd, glm::mat4 mvp, VkDeviceAddress vertexBuffer)
{
    mShadowPCData.LightMVP = mvp;
    mShadowPCData.VertexBuffer = vertexBuffer;

    vkCmdPushConstants(cmd, mOpaquePipeline.Layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                       sizeof(mShadowPCData), &mShadowPCData);
}

void ShadowmapHandler::PushConstantAlpha(VkCommandBuffer cmd, glm::mat4 mvp, VkDeviceAddress vertexBuffer)
{
    mShadowPCData.LightMVP = mvp;
    mShadowPCData.VertexBuffer = vertexBuffer;

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