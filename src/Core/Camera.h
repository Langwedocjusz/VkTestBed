#pragma once

#include "Bitflags.h"

#include <glm/glm.hpp>

class Camera {
  public:
    Camera() = default;

    void OnUpdate(float deltatime, uint32_t width, uint32_t height);
    void OnImGui();

    void OnKeyPressed(int keycode, bool repeat);
    void OnKeyReleased(int keycode);
    void OnMouseMoved(float x, float y);

    [[nodiscard]] glm::vec3 GetPos() const
    {
        return mPos;
    }

    [[nodiscard]] glm::mat4 GetProj() const
    {
        return mProj;
    }
    [[nodiscard]] glm::mat4 GetView() const
    {
        return mView;
    }

    [[nodiscard]] glm::mat4 GetViewProj() const
    {
        return mViewProj;
    }

    [[nodiscard]] glm::mat4 GetInvViewProj() const
    {
        return mInvViewProj;
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

    uint32_t mWidth, mHeight;

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
    glm::mat4 mViewProj;
    glm::mat4 mInvViewProj;
};