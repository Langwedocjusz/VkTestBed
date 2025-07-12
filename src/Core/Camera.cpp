#include "Pch.h"
#include "Camera.h"

#include "BufferUtils.h"
#include "Descriptor.h"
#include "Keycodes.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include <optional>

Camera::Camera(VulkanContext &ctx, FrameInfo &info)
    : mCtx(ctx), mFrame(info), mMainDeletionQueue(ctx)
{
    // Create descriptor sets:
    //  Descriptor layout
    mDescriptorSetLayout =
        DescriptorSetLayoutBuilder("CameraDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .Build(ctx, mMainDeletionQueue);

    // Descriptor pool
    uint32_t maxSets = mFrame.MaxInFlight;

    std::vector<VkDescriptorPoolSize> poolCounts{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets},
    };

    mDescriptorPool = Descriptor::InitPool(ctx, maxSets, poolCounts);
    mMainDeletionQueue.push_back(mDescriptorPool);

    // Descriptor sets allocation
    std::vector<VkDescriptorSetLayout> layouts(mFrame.MaxInFlight, mDescriptorSetLayout);

    mDescriptorSets = Descriptor::Allocate(ctx, mDescriptorPool, layouts);

    // Create Uniform Buffers:
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    mUniformBuffers.resize(mFrame.MaxInFlight);

    for (auto &uniformBuffer : mUniformBuffers)
    {
        uniformBuffer = MakeBuffer::MappedUniform(ctx, "CameraUniformBuffer", bufferSize);
        mMainDeletionQueue.push_back(uniformBuffer);
    }

    // Update descriptor sets:
    for (size_t i = 0; i < mDescriptorSets.size(); i++)
    {
        DescriptorUpdater(mDescriptorSets[i])
            .WriteUniformBuffer(0, mUniformBuffers[i].Handle, sizeof(UniformBufferObject))
            .Update(ctx);
    }
}

Camera::~Camera()
{
    mMainDeletionQueue.flush();
}

void Camera::OnUpdate(float deltatime)
{
    ProcessKeyboard(deltatime);

    // Vectors are not updated here, since they only change on mouse input
    // which is handled on event.

    mView = glm::lookAt(mPos, mPos + mFront, mUp);

    mProj = ProjPerspective();

    // To compensate for change of orientation between
    // OpenGL and Vulkan:
    mProj[1][1] *= -1;

    // Upload data to uniform buffer:
    mUBOData.ViewProjection = mProj * mView;

    auto &uniformBuffer = mUniformBuffers[mFrame.Index];
    Buffer::UploadToMapped(uniformBuffer, &mUBOData, sizeof(mUBOData));
}

glm::mat4 Camera::ProjPerspective()
{
    auto width = static_cast<float>(mCtx.Swapchain.extent.width);
    auto height = static_cast<float>(mCtx.Swapchain.extent.height);

    float aspect = width / height;

    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
}

glm::mat4 Camera::ProjOrthogonal()
{
    auto width = static_cast<float>(mCtx.Swapchain.extent.width);
    auto height = static_cast<float>(mCtx.Swapchain.extent.height);

    float sx = 1.0f, sy = 1.0f;

    if (height < width)
        sx = width / height;
    else
        sy = height / width;

    return glm::ortho(-sx, sx, -sy, sy, -1.0f, 1.0f);
}

void Camera::OnImGui()
{
}

void Camera::UpdateVectors()
{
    glm::mat3 rot = glm::yawPitchRoll(glm::radians(mYaw), glm::radians(mPitch), 0.0f);

    mFront = rot * glm::vec3(0, 0, 1);
    mRight = glm::normalize(glm::cross(mFront, mWorldUp));
    mUp = glm::normalize(glm::cross(mRight, mFront));
}

void Camera::ProcessKeyboard(float deltatime)
{
    float deltaPos = deltatime * mSpeed;

    if (mMovementFlags[Movement::Forward])
        mPos += deltaPos * mFront;

    if (mMovementFlags[Movement::Backward])
        mPos -= deltaPos * mFront;

    if (mMovementFlags[Movement::Left])
        mPos -= deltaPos * mRight;

    if (mMovementFlags[Movement::Right])
        mPos += deltaPos * mRight;
}

void Camera::ProcessMouse(float xoffset, float yoffset)
{
    mPitch += mSensitivity * yoffset;
    mYaw += mSensitivity * xoffset;

    if (mPitch > 89.0f)
        mPitch = 89.0f;
    if (mPitch < -89.0f)
        mPitch = -89.0f;

    UpdateVectors();
}

static std::optional<Camera::Movement> KeyToMovement(int keycode)
{
    switch (keycode)
    {
    case VKTB_KEY_W:
        return Camera::Movement::Forward;
    case VKTB_KEY_S:
        return Camera::Movement::Backward;
    case VKTB_KEY_A:
        return Camera::Movement::Left;
    case VKTB_KEY_D:
        return Camera::Movement::Right;
    }

    return {};
}

void Camera::OnKeyPressed(int keycode, bool repeat)
{
    if (repeat)
        return;

    auto movement = KeyToMovement(keycode);

    if (movement.has_value())
        mMovementFlags.Set(movement.value());
}

void Camera::OnKeyReleased(int keycode)
{
    auto movement = KeyToMovement(keycode);

    if (movement.has_value())
        mMovementFlags.Unset(movement.value());
}

void Camera::OnMouseMoved(float x, float y)
{
    auto width = static_cast<float>(mCtx.Swapchain.extent.width);
    auto height = static_cast<float>(mCtx.Swapchain.extent.height);

    float xpos = x / width;
    float ypos = y / height;

    if (mMouseInit)
    {
        mMouseLastX = xpos;
        mMouseLastY = ypos;
        mMouseInit = false;
    }

    float xoffset = xpos - mMouseLastX;
    float yoffset = mMouseLastY - ypos;

    mMouseLastX = xpos;
    mMouseLastY = ypos;

    const float max_offset = 0.1f;

    if (std::abs(xoffset) > max_offset)
        xoffset = (xoffset > 0.0f) ? max_offset : -max_offset;

    if (std::abs(yoffset) > max_offset)
        yoffset = (yoffset > 0.0f) ? max_offset : -max_offset;

    ProcessMouse(xoffset, yoffset);
}