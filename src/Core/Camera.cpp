#include "Camera.h"
#include "Pch.h"

#include "Keycodes.h"
#include "Vassert.h"

#include "glm/ext/matrix_clip_space.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include <imgui.h>

#include <optional>
#include <utility>

void Camera::OnUpdate(float deltatime, uint32_t width, uint32_t height)
{
    mWidth = width;
    mHeight = height;

    ProcessKeyboard(deltatime);

    // Vectors are not updated here, since they only change on mouse input
    // which is handled on event.

    mView = glm::lookAt(mPos, mPos + mFront, mUp);
    mProj = ProjPerspective();

    // To compensate for change of orientation between
    // OpenGL and Vulkan:
    mProj[1][1] *= -1;

    mViewProj = mProj * mView;
    mInvViewProj = glm::inverse(mViewProj);

    auto GetCorner = [&](glm::vec4 v) {
        auto res = mInvViewProj * v;
        return res / res.w;
    };

    mFrustum = Frustum{
        .NearTopLeft = GetCorner(glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f)),
        .NearTopRight = GetCorner(glm::vec4(1.0f, -1.0f, 0.0f, 1.0f)),
        .NearBottomLeft = GetCorner(glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f)),
        .NearBottomRight = GetCorner(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)),
        .FarTopLeft = GetCorner(glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f)),
        .FarTopRight = GetCorner(glm::vec4(1.0f, -1.0f, 1.0f, 1.0f)),
        .FarBottomLeft = GetCorner(glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f)),
        .FarBottomRight = GetCorner(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)),
    };

    // Not doing the final division makes
    // the cubemap coordinates more stable:
    mFrustumBack = FrustumBack{
        .TopLeft = mInvViewProj * glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f),
        .TopRight = mInvViewProj * glm::vec4(1.0f, -1.0f, 1.0f, 1.0f),
        .BottomLeft = mInvViewProj * glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f),
        .BottomRight = mInvViewProj * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
    };
}

void Camera::OnEvent(Event::EventVariant event)
{
    if (std::holds_alternative<Event::Key>(event))
    {
        auto e = std::get<Event::Key>(event);

        if (e.Action == VKTB_PRESS || e.Action == VKTB_REPEAT)
        {
            bool repeat = e.Action == VKTB_REPEAT;
            OnKeyPressed(e.Keycode, repeat);
        }
        else if (e.Action == VKTB_RELEASE)
        {
            OnKeyReleased(e.Keycode);
        }
    }

    else if (std::holds_alternative<Event::CursorPos>(event))
    {
        auto e = std::get<Event::CursorPos>(event);
        OnMouseMoved(static_cast<float>(e.XPos), static_cast<float>(e.YPos));
    }
}

glm::mat4 Camera::ProjPerspective()
{
    const float aspect = static_cast<float>(mWidth) / static_cast<float>(mHeight);

    return glm::perspective(mFovRadians, aspect, mZMin, mZMax);
}

glm::mat4 Camera::ProjOrthogonal()
{
    auto width = static_cast<float>(mWidth);
    auto height = static_cast<float>(mHeight);

    float sx = 1.0f, sy = 1.0f;

    if (height < width)
        sx = width / height;
    else
        sy = height / width;

    return glm::ortho(-sx, sx, -sy, sy, -1.0f, 1.0f);
}

glm::mat4 Camera::GetViewProjRestrictedRange(float xmin, float xmax, float ymin,
                                             float ymax) const
{
    // Default perspective camera near plane:
    const float aspect = static_cast<float>(mWidth) / static_cast<float>(mHeight);

    const float camTop = glm::tan(0.5f * mFovRadians) * mZMin;
    const float camBottom = -camTop;
    const float camRight = aspect * camTop;
    const float camLeft = -camRight;

    // Rectangle covering provided limits:
    auto lerp = [](float x, float y, float t) { return (1.0f - t) * x + t * y; };

    float left = lerp(camLeft, camRight, xmin);
    float right = lerp(camLeft, camRight, xmax);
    float bottom = lerp(camBottom, camTop, ymin);
    float top = lerp(camBottom, camTop, ymax);

    // Construct projection for given frustum:
    // auto proj = glm::frustumLH_ZO(left, right, bottom, top, mZMin, mZMax);
    auto proj = glm::frustum(left, right, bottom, top, mZMin, mZMax);
    proj[1][1] *= -1.0f;

    return proj * mView;
}

void Camera::OnImGui()
{
    // ImGui::Begin("Camera");
    //
    // ImGui::Text("NearTopLeft: (%f, %f, %f)", mFrustum.NearTopLeft.x,
    // mFrustum.NearTopLeft.y, mFrustum.NearTopLeft.z); ImGui::Text("NearTopRight: (%f,
    // %f, %f)", mFrustum.NearTopRight.x, mFrustum.NearTopRight.y,
    // mFrustum.NearTopRight.z); ImGui::Text("NearBottomLeft: (%f, %f, %f)",
    // mFrustum.NearBottomLeft.x, mFrustum.NearBottomLeft.y, mFrustum.NearBottomLeft.z);
    // ImGui::Text("NearBottomRight: (%f, %f, %f)", mFrustum.NearBottomRight.x,
    // mFrustum.NearBottomRight.y, mFrustum.NearBottomRight.z); ImGui::Text("FarTopLeft:
    // (%f, %f, %f)", mFrustum.FarTopLeft.x, mFrustum.FarTopLeft.y,
    // mFrustum.FarTopLeft.z); ImGui::Text("FarTopRight: (%f, %f, %f)",
    // mFrustum.FarTopRight.x, mFrustum.FarTopRight.y, mFrustum.FarTopRight.z);
    // ImGui::Text("FarBottomLeft: (%f, %f, %f)", mFrustum.FarBottomLeft.x,
    // mFrustum.FarBottomLeft.y, mFrustum.FarBottomLeft.z); ImGui::Text("FarBottomRight:
    // (%f, %f, %f)", mFrustum.FarBottomRight.x, mFrustum.FarBottomRight.y,
    // mFrustum.FarBottomRight.z);
    //
    // ImGui::End();
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
    default:
        return {};
    }

    //On msvc, debug mode, std::unreachable triggers an 
    //'unreachable code' exception...
    #ifndef _MSC_VER
    std::unreachable();
    #endif
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
    float xpos = x / static_cast<float>(mWidth);
    float ypos = y / static_cast<float>(mHeight);

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