#pragma once

#include "Bitflags.h"
#include "Buffer.h"
#include "DeletionQueue.h"
#include "Frame.h"
#include "VulkanContext.h"

#include <glm/glm.hpp>

class Camera {
  public:
    Camera(VulkanContext &ctx, FrameInfo &info);
    ~Camera();

    void OnUpdate(float deltatime);
    void OnImGui();

    [[nodiscard]] VkDescriptorSetLayout DescriptorSetLayout() const
    {
        return mDescriptorSetLayout;
    }

    // To-do: figure out a better way of doing this:
    [[nodiscard]] VkDescriptorSet *DescriptorSet()
    {
        return &mDescriptorSets[mFrame.Index];
    }

    void OnKeyPressed(int keycode, bool repeat);
    void OnKeyReleased(int keycode);
    void OnMouseMoved(float x, float y);

    [[nodiscard]] glm::mat4 GetProj() const
    {
        return mProj;
    }
    [[nodiscard]] glm::mat4 GetView() const
    {
        return mView;
    }

    enum class Movement : uint8_t
    {
        Forward,
        Backward,
        Left,
        Right
    };

  private:
    glm::mat4 ProjPerspective();
    glm::mat4 ProjOrthogonal();

    void ProcessKeyboard(float deltatime);
    void ProcessMouse(float xoffset, float yoffset);
    void UpdateVectors();

  private:
    Bitflags<Movement> mMovementFlags;

    float mSpeed = 1.0f, mSensitivity = 100.0f;

    glm::vec3 mPos{0.0f, 0.0f, 0.0f};
    float mYaw = 0.0f, mPitch = 0.0f;

    const glm::vec3 mWorldUp{0.0f, -1.0f, 0.0f};
    glm::vec3 mFront{0.0f, 0.0f, 1.0f};
    glm::vec3 mRight{1.0f, 0.0f, 0.0f};
    glm::vec3 mUp{0.0f, -1.0f, 0.0f};

    bool mMouseInit = true;
    float mMouseLastX = 0.0f, mMouseLastY = 0.0f;

    glm::mat4 mProj;
    glm::mat4 mView;

  private:
    VulkanContext &mCtx;
    FrameInfo &mFrame;

    VkDescriptorSetLayout mDescriptorSetLayout;
    VkDescriptorPool mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;

    struct UniformBufferObject {
        glm::mat4 ViewProjection = glm::mat4(1.0f);
    };
    UniformBufferObject mUBOData;

    std::vector<Buffer> mUniformBuffers;

    DeletionQueue mMainDeletionQueue;
};