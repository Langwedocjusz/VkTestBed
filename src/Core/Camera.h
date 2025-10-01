#pragma once

#include "Bitflags.h"
#include "Event.h"
#include "glm/trigonometric.hpp"

#include <glm/glm.hpp>

// For calculating collisions in worldspace:
struct Frustum {
    glm::vec4 NearTopLeft;
    glm::vec4 NearTopRight;
    glm::vec4 NearBottomLeft;
    glm::vec4 NearBottomRight;
    glm::vec4 FarTopLeft;
    glm::vec4 FarTopRight;
    glm::vec4 FarBottomLeft;
    glm::vec4 FarBottomRight;

    [[nodiscard]] std::array<glm::vec4, 8> GetVertices() const
    {
        return {NearTopLeft, NearTopRight, NearBottomLeft, NearBottomRight,
                FarTopLeft,  FarTopRight,  FarBottomLeft,  FarBottomRight};
    }

    void SetVertices(std::array<glm::vec4, 8> vertices)
    {
        NearTopLeft = vertices[0];
        NearTopRight = vertices[1];
        NearBottomLeft = vertices[2];
        NearBottomRight = vertices[3];
        FarTopLeft = vertices[4];
        FarTopRight = vertices[5];
        FarBottomLeft = vertices[6];
        FarBottomRight = vertices[7];
    }
};

// For getting cubemap coordinates for background:
struct FrustumBack {
    glm::vec4 TopLeft;
    glm::vec4 TopRight;
    glm::vec4 BottomLeft;
    glm::vec4 BottomRight;
};

class Camera {
  public:
    Camera() = default;

    void OnUpdate(float deltatime, uint32_t width, uint32_t height);
    void OnImGui();
    void OnEvent(Event::EventVariant event);

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

    [[nodiscard]] const Frustum &GetFrustum() const
    {
        return mFrustum;
    }

    [[nodiscard]] const FrustumBack &GetFrustumBack() const
    {
        return mFrustumBack;
    }

    /// Get perspective projection covering a fiven rectangle of the near plane.
    /// Rectangle coords are meant to be normalized to [0,1].
    [[nodiscard]] glm::mat4 GetViewProjRestrictedRange(float xmin, float xmax, float ymin,
                                                       float ymax) const;

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

    void OnKeyPressed(int keycode, bool repeat);
    void OnKeyReleased(int keycode);
    void OnMouseMoved(float x, float y);

    void ProcessKeyboard(float deltatime);
    void ProcessMouse(float xoffset, float yoffset);
    void UpdateVectors();

  private:
    Bitflags<Movement> mMovementFlags;

    uint32_t mWidth, mHeight;
    float mZMin = 0.01f, mZMax = 1000.0f, mFovRadians = glm::radians(45.0f);

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

    Frustum mFrustum;
    FrustumBack mFrustumBack;
};