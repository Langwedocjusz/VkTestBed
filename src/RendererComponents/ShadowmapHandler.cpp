#include "ShadowmapHandler.h"
#include "Pch.h"

#include "Barrier.h"
#include "Common.h"
#include "Descriptor.h"
#include "ImageUtils.h"
#include "Sampler.h"

#include "imgui.h"

#include <imgui_impl_vulkan.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <limits>

static std::optional<glm::vec3> IntersectFace(std::array<glm::vec3, 2> lineSegment, std::array<glm::vec3,4> face);

ShadowmapHandler::ShadowmapHandler(VulkanContext &ctx)
    : mCtx(ctx), mMainDeletionQueue(ctx), mPipelineDeletionQueue(ctx)
{
    mSamplerShadowmap = SamplerBuilder("MinimalPbrSamplerShadowmap")
                            .SetMagFilter(VK_FILTER_LINEAR)
                            .SetMinFilter(VK_FILTER_LINEAR)
                            .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
                            .SetBorderColor(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE)
                            .SetCompareOp(VK_COMPARE_OP_LESS)
                            .Build(mCtx, mMainDeletionQueue);

    // Create static descriptor pool:
    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        };

        mStaticDescriptorPool = Descriptor::InitPool(mCtx, 1, poolCounts);
        mMainDeletionQueue.push_back(mStaticDescriptorPool);
    }

    // Create descriptor set layout for sampling the shadowmap
    // and allocate the corresponding descriptor set:

    mShadowmapDescriptorSetLayout =
        DescriptorSetLayoutBuilder("MinimalPBRShadowmapDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mMainDeletionQueue);

    mShadowmapDescriptorSet =
        Descriptor::Allocate(mCtx, mStaticDescriptorPool, mShadowmapDescriptorSetLayout);

    // Create the shadowmap:
    Image2DInfo shadowmapInfo{
        .Extent = {mShadowmapResolution, mShadowmapResolution},
        .Format = mShadowmapFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = 1,
        .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    mShadowmap = MakeImage::Image2D(mCtx, "Shadowmap", shadowmapInfo);
    mShadowmapView = MakeView::View2D(mCtx, "ShadowmapView", mShadowmap, mShadowmapFormat,
                                      VK_IMAGE_ASPECT_DEPTH_BIT);

    mMainDeletionQueue.push_back(mShadowmap);
    mMainDeletionQueue.push_back(mShadowmapView);

    // Update the shadowmap descriptor:
    DescriptorUpdater(mShadowmapDescriptorSet)
        .WriteImageSampler(0, mShadowmapView, mSamplerShadowmap)
        .Update(mCtx);

    // For debug view in imgui:
    mSamplerDebug = SamplerBuilder("MinimalPbrSampler2D")
                        .SetMagFilter(VK_FILTER_LINEAR)
                        .SetMinFilter(VK_FILTER_LINEAR)
                        .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                        .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                        .SetMaxLod(12.0f)
                        .Build(mCtx, mMainDeletionQueue);

    mDebugTextureDescriptorSet = ImGui_ImplVulkan_AddTexture(
        mSamplerDebug, mShadowmapView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

ShadowmapHandler::~ShadowmapHandler()
{
    mPipelineDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void ShadowmapHandler::RebuildPipelines(const Vertex::Layout &vertexLayout,
                                        VkDescriptorSetLayout materialDSLayout)
{
    mPipelineDeletionQueue.flush();

    mShadowmapPipeline = PipelineBuilder("MinimalPBRShadowmapPipeline")
                             .SetShaderPathVertex("assets/spirv/ShadowmapVert.spv")
                             .SetShaderPathFragment("assets/spirv/ShadowmapFrag.spv")
                             .SetVertexInput(vertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
                             .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                             .SetPolygonMode(VK_POLYGON_MODE_FILL)
                             .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
                             .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
                             .SetPushConstantSize(sizeof(mShadowPCData))
                             .AddDescriptorSetLayout(materialDSLayout)
                             .EnableDepthTest()
                             .SetDepthFormat(mShadowmapFormat)
                             .Build(mCtx, mPipelineDeletionQueue);
}

void ShadowmapHandler::OnUpdate(Frustum camFr, glm::vec3 lightDir, AABB sceneAABB)
{
    // Construct light view matrix, looking along the light direction:
    // Up vector doesn't really matter.
    glm::mat4 lightView = glm::lookAt(lightDir, glm::vec3(0.0f), glm::vec3(0,-1,0));

    // Construct worldspace coords for the shadow sampling part of the frustum:
    auto ScaleFarVector = [this](glm::vec4 near, glm::vec4 &far) {
        auto normalized = glm::normalize(glm::vec3(far - near));
        far = near + mShadowDist * glm::vec4(normalized, 0.0f);
    };

    Frustum shadowFrustum = camFr;
    ScaleFarVector(shadowFrustum.NearTopLeft, shadowFrustum.FarTopLeft);
    ScaleFarVector(shadowFrustum.NearTopRight, shadowFrustum.FarTopRight);
    ScaleFarVector(shadowFrustum.NearBottomLeft, shadowFrustum.FarBottomLeft);
    ScaleFarVector(shadowFrustum.NearBottomRight, shadowFrustum.FarBottomRight);

    // Transform the shadowed frustum to light view space:
    auto frustumVertices = shadowFrustum.GetVertices();

    for (auto &vert : frustumVertices)
    {
        //On paper it seems like y coord of the frustum should be flipped
        //Debug view of the shadowmap also makes more sense when it is done.
        //But actual shadows in the viewport work better without the filp.
        //Introducing the flip means sometimes fragments withing frustum (close to camera)
        //end up sampling outside the shadowmap.
        //TODO: Understand why.

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
    float texelSizeX = (maxX - minX) / mShadowmapResolution;
    float texelSizeY = (maxY - minY) / mShadowmapResolution;

    minX = std::floor(minX / texelSizeX) * texelSizeX;
    maxX = std::floor(maxX / texelSizeX) * texelSizeX;
    minY = std::floor(minY / texelSizeY) * texelSizeY;
    maxY = std::floor(maxY / texelSizeY) * texelSizeY;

    // Update minZ and maxZ values based on scene bounding box:
    //if (mConservativeFrustum)
    {
        auto verts = sceneAABB.GetVertices();

        for (auto& vert : verts)
        {
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
    /*else
    {
        //TODO: Intersection of the AABB with extruded frustum for less conservative Z bounds

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

    // Construct the projection matrix:
    glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

    mLightViewProj = lightProj * lightView;
}

void ShadowmapHandler::OnImGui()
{
    ImGui::SliderFloat("Shadow Dist", &mShadowDist, 1.0f, 40.0f);
    //ImGui::Checkbox("Conservative", &mConservativeFrustum);

    ImGui::Image((ImTextureID)mDebugTextureDescriptorSet, ImVec2(512, 512));
}

void ShadowmapHandler::BeginShadowPass(VkCommandBuffer cmd)
{
    barrier::ImageBarrierDepthToRender(cmd, mShadowmap.Handle);

    auto extent = VkExtent2D(mShadowmap.Info.extent.width, mShadowmap.Info.extent.height);

    common::BeginRenderingDepth(cmd, extent, mShadowmapView, false, true);

    mShadowmapPipeline.Bind(cmd);
    common::ViewportScissor(cmd, extent);
}

void ShadowmapHandler::EndShadowPass(VkCommandBuffer cmd)
{
    vkCmdEndRendering(cmd);
    barrier::ImageBarrierDepthToSample(cmd, mShadowmap.Handle);
}

void ShadowmapHandler::PushConstantTransform(VkCommandBuffer cmd, glm::mat4 transform)
{
    mShadowPCData.LightMVP = mLightViewProj * transform;

    vkCmdPushConstants(cmd, mShadowmapPipeline.Layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                       sizeof(mShadowPCData), &mShadowPCData);
}

void ShadowmapHandler::BindMaterialDS(VkCommandBuffer cmd, VkDescriptorSet materialDS)
{
    mShadowmapPipeline.BindDescriptorSet(cmd, materialDS, 0);
}

/*static std::optional<glm::vec3> IntersectFace(std::array<glm::vec3, 2> lineSegment, std::array<glm::vec3,4> face)
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