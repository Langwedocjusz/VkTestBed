#include "Application.h"
#include "Pch.h"

#include "Event.h"
#include "ImGuiInit.h"
#include "Keycodes.h"
#include "RenderContext.h"
#include "SceneGui.h"
#include "ShaderManager.h"
#include "SystemWindow.h"
#include "VulkanContext.h"

#include <optional>
#include <tracy/Tracy.hpp>

#include "imgui.h"

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

    std::optional<ImVec2> mPickRequested = std::nullopt;

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
    mWindow.SetEventCallback([](void *usrPtr, Event::EventVariant event) {
        auto app = reinterpret_cast<Impl *>(usrPtr);
        app->OnEvent(event);
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
            mRender.ResizeSwapchain();
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

        // Handle object picking if requested:
        if (mPickRequested.has_value())
        {
            auto pos = *mPickRequested;
            mPickRequested = std::nullopt;

            auto pickedId = mRender.PickObjectId(pos.x, pos.y);
            mSceneGui.SetSelection(pickedId);
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
    // Handle some key combinations before propagating further:
    if (std::holds_alternative<Event::Key>(event))
    {
        auto keyEvent = std::get<Event::Key>(event);

        // Toggle cursor capture state on escape being pressed:
        if (keyEvent.Keycode == VKTB_KEY_ESCAPE && keyEvent.Action == VKTB_PRESS)
        {
            mCursorCaptured = !mCursorCaptured;

            if (mCursorCaptured)
                mWindow.CaptureCursor();
            else
                mWindow.FreeCursor();
        }

        // Hold ctrl pressed state
        // To do: implement better solution (buffer for whole keyboard..)
        static bool ctrlPressed = false;

        if (keyEvent.Keycode == VKTB_KEY_LEFT_CONTROL)
        {
            if (keyEvent.Action == VKTB_PRESS)
                ctrlPressed = true;
            else if (keyEvent.Action == VKTB_RELEASE)
                ctrlPressed = false;
        }

        bool scaleUI = false;
        const float scaleStep = 0.05f;
        static float scaleFac = 1.0f;

        if (ctrlPressed && keyEvent.Keycode == VKTB_KEY_EQUAL)
        {
            scaleFac += scaleStep;
            scaleUI = true;
        }

        if (ctrlPressed && keyEvent.Keycode == VKTB_KEY_MINUS)
        {
            scaleFac = std::max(scaleStep, scaleFac - scaleStep);
            scaleUI = true;
        }

        if (scaleUI)
        {
            iminit::ScaleStyle(scaleFac);
        }
    }

    // If cursor caputured - propagate to rendered event handling:
    if (mCursorCaptured)
    {
        if (std::holds_alternative<Event::Key>(event))
        {
            auto e = std::get<Event::Key>(event);

            if (e.Action == VKTB_PRESS || e.Action == VKTB_REPEAT)
            {
                bool repeat = e.Action == VKTB_REPEAT;
                mRender.OnKeyPressed(e.Keycode, repeat);
            }
            else if (e.Action == VKTB_RELEASE)
            {
                mRender.OnKeyReleased(e.Keycode);
            }
        }

        else if (std::holds_alternative<Event::CursorPos>(event))
        {
            auto e = std::get<Event::CursorPos>(event);
            mRender.OnMouseMoved(static_cast<float>(e.XPos), static_cast<float>(e.YPos));
        }
    }

    // If cursor not captured - use imgui event handling:
    else
    {
        iminit::ImGuiHandleEvent(event);

        // Detect if background was clicked
        // To-do: encapsulate this somehow
        if (std::holds_alternative<Event::MouseButton>(event))
        {
            auto mbEvent = std::get<Event::MouseButton>(event);

            if (mbEvent.Button == VKTB_MOUSE_BUTTON_LEFT && mbEvent.Action == VKTB_PRESS)
            {
                if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
                {
                    // To-do: when input buffers are implemented maybe fetch
                    // mouse pos from there, instead of calling to imgui
                    mPickRequested = ImGui::GetMousePos();
                    // printf("Background clicked, with cursor at: (%f, %f)\n", pos.x,
                    //        pos.y);
                }
            }
        }
    }
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