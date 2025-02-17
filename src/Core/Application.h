#pragma once

#include "Event.h"
#include "RenderContext.h"
#include "SceneEditor.h"
#include "ShaderManager.h"
#include "SystemWindow.h"
#include "VulkanContext.h"

#include <chrono>

class Application {
  public:
    Application();
    ~Application();

    void Run();

    void OnResize(uint32_t width, uint32_t height);
    void OnEvent(Event::EventVariant event);

  private:
    SystemWindow mWindow;
    VulkanContext mCtx;

    ShaderManager mShaderManager;

    Scene mScene;
    SceneEditor mSceneEditor;

    RenderContext mRender;

    float mDeltaTime = 0.0f;
    std::chrono::time_point<std::chrono::high_resolution_clock> mOldTime;

    bool mCursorCaptured = false;
};