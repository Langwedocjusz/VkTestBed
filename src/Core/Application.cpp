#include "Application.h"
#include "Pch.h"

#include "CppUtils.h"
#include "Event.h"
#include "ImGuiInit.h"
#include "Keycodes.h"
#include "RenderContext.h"
#include "SceneGui.h"
#include "ShaderManager.h"
#include "SystemWindow.h"
#include "VulkanContext.h"

#include <tracy/Tracy.hpp>

#include <chrono>
#include <variant>

class Application::Impl {
  public:
    Impl();
    ~Impl() = default;

    void Run();

    void OnResize(uint32_t width, uint32_t height);
    void OnEvent(Event::EventVariant event);

  private:
    SystemWindow mWindow;
    VulkanContext mCtx;

    ShaderManager mShaderManager;
    RenderContext mRender;

    Scene mScene;
    SceneEditor mSceneEditor;
    SceneGui mSceneGui;

    float mDeltaTime = 0.0f;
    std::chrono::time_point<std::chrono::high_resolution_clock> mOldTime;

    bool mCursorCaptured = false;
    bool mStillResizing = false;
    bool mResizeRequested = false;
};

Application::Impl::Impl()
    : mWindow(800, 600, "Vulkanik", static_cast<void *>(this)),
      mCtx(800, 600, "VkTestBed", mWindow),
      mShaderManager("assets/shaders", "assets/spirv"), mRender(mCtx),
      mSceneEditor(mScene), mSceneGui(mSceneEditor)
{
    mWindow.SetFramebufferResizeCallback([](void *usr_ptr, int width, int height) {
        auto app = reinterpret_cast<Impl *>(usr_ptr);
        app->OnResize(width, height);
    });

    mWindow.SetKeyCallback([](void *usr_ptr, int key, int action) {
        auto app = reinterpret_cast<Impl *>(usr_ptr);

        switch (action)
        {
        case VKTB_PRESS: {
            app->OnEvent(Event::KeyPressed(key, false));
            break;
        }
        case VKTB_REPEAT: {
            app->OnEvent(Event::KeyPressed(key, true));
            break;
        }
        case VKTB_RELEASE: {
            app->OnEvent(Event::KeyReleased(key));
            break;
        }
        default: {
            break;
        }
        }
    });

    mWindow.SetMouseMovedCallback([](void *usr_ptr, float xPos, float yPos) {
        auto app = reinterpret_cast<Impl *>(usr_ptr);
        app->OnEvent(Event::MouseMoved(xPos, yPos));
    });

    iminit::InitImGui();
    iminit::InitGlfwBackend(mWindow.Get());
    mRender.OnInit();

    // First-time scene loading
    mRender.LoadScene(mScene);
    mScene.ClearUpdateFlags();
}

void Application::Impl::Run()
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
        mRender.OnRender(mSceneGui.GetSelection());

        // Tracy profiler:
        FrameMark;
    }

    vkDeviceWaitIdle(mCtx.Device);
    iminit::DestroyImGui();
}

void Application::Impl::OnResize(uint32_t width, uint32_t height)
{
    mCtx.RequestedWidth = width;
    mCtx.RequestedHeight = height;

    mStillResizing = true;
    mResizeRequested = true;
}

void Application::Impl::OnEvent(Event::EventVariant event)
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

Application::Application() : mPImpl(std::make_unique<Impl>())
{
}

Application::~Application()
{
}

void Application::Run()
{
    mPImpl->Run();
}