#include "Application.h"

#include "CppUtils.h"
#include "Event.h"
#include "ImGuiInit.h"
#include "Keycodes.h"

#include <tracy/Tracy.hpp>

#include <iostream>
#include <variant>

Application::Application()
    : mWindow(800, 600, "Vulkanik", static_cast<void *>(this)),
      mCtx(800, 600, "VkTestBed", mWindow),
      mShaderManager("assets/shaders", "assets/spirv"), mRender(mCtx),
      mSceneEditor(mScene), mSceneGui(mSceneEditor)
{
    iminit::InitImGui();
    iminit::InitGlfwBackend(mWindow.Get());
    mRender.InitImGuiVulkanBackend();

    // First-time scene loading
    mRender.LoadScene(mScene);
    mScene.ClearUpdateFlags();
}

Application::~Application()
{
}

void Application::Run()
{
    while (!mWindow.ShouldClose())
    {
        // Update delta time:
        using ms = std::chrono::duration<float, std::milli>;

        auto currentTime = std::chrono::high_resolution_clock::now();
        mDeltaTime = std::chrono::duration_cast<ms>(currentTime - mOldTime).count();
        mOldTime = currentTime;

        // Recreate swapchain and related resources if necessary:
        bool handleResize = (!mStillResizing && mResizeRequested);
        mStillResizing = false;

        if (!mCtx.SwapchainOk || handleResize)
        {
            mRender.ResizeSwapchan();
            mResizeRequested = false;
        }

        // Reload the scene if necessary:
        if (mScene.UpdateRequested())
            mRender.LoadScene(mScene);

        // Reload shaders if necessary:
        if (mShaderManager.CompilationScheduled())
        {
            mShaderManager.CompileToBytecode();
            mRender.RebuildPipelines();
        }

        // Poll system events:
        mWindow.PollEvents();

        // Update Renderer and Scene Editor
        mRender.OnUpdate(mDeltaTime / 1000.0f);
        mSceneEditor.OnUpdate();

        // Collect imgui calls
        iminit::BeginGuiFrame();
        mSceneGui.OnImGui();
        mRender.OnImGui();
        iminit::FinalizeGuiFrame();

        // Render things:
        mRender.OnRender();

        // Tracy profiler:
        FrameMark;
    }

    vkDeviceWaitIdle(mCtx.Device);
    iminit::DestroyImGui();
}

void Application::OnResize(uint32_t width, uint32_t height)
{
    mCtx.RequestedWidth = width;
    mCtx.RequestedHeight = height;

    mStillResizing = true;
    mResizeRequested = true;
}

void Application::OnEvent(Event::EventVariant event)
{
    // Handle esc being pressed:
    if (std::holds_alternative<Event::KeyPressed>(event))
    {
        if (std::get<Event::KeyPressed>(event).Keycode == VKTB_KEY_ESCAPE)
        {
            mCursorCaptured = !mCursorCaptured;

            if (mCursorCaptured)
                mWindow.CaptureCursor();
            else
                mWindow.FreeCursor();
        }
    }

    // If cursor not caputured, early bail - use Imgui event handling
    if (!mCursorCaptured)
        return;

    // If not, do event handling
    // To-do: prevent events from propagating to imgui in this case:
    std::visit(
        overloaded{[this](Event::KeyPressed arg) {
                       mRender.OnKeyPressed(arg.Keycode, arg.Repeat);
                   },
                   [this](Event::KeyReleased arg) { mRender.OnKeyReleased(arg.Keycode); },
                   [this](Event::MouseMoved arg) { mRender.OnMouseMoved(arg.X, arg.Y); }},
        event);
}
