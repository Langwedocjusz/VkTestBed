#include "Application.h"
#include <memory>

#include "Event.h"
#include "ImGuiUtils.h"
#include "Keycodes.h"
#include "Primitives.h"

#include <variant>

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

Application::Application()
    : mWindow(800, 600, "Vulkanik", static_cast<void *>(this)),
      mCtx(800, 600, "VkTestBed", mWindow), mRender(mCtx)
{
    imutils::InitImGui();
    imutils::InitGlfwBackend(mWindow.Get());
    mRender.InitImGuiVulkanBackend();

    mScene = std::make_unique<Scene>();

    mScene->Objects.emplace_back();
    mScene->Objects.back().Provider = primitive::ColoredCube(glm::vec3(0.5f));
    mScene->Objects.back().Instances.push_back(InstanceData{});

    // First-time scene loading
    mRender.LoadScene(*mScene);
    mScene->ClearUpdateFlags();
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
        if (!mCtx.SwapchainOk)
        {
            vkDeviceWaitIdle(mCtx.Device);

            mRender.DestroySwapchainResources();
            mCtx.CreateSwapchain();
            mRender.CreateSwapchainResources();

            mCtx.SwapchainOk = true;
        }

        // Reload the scene if necessary:
        if (mScene->UpdateRequested)
        {
            vkDeviceWaitIdle(mCtx.Device);

            mRender.LoadScene(*mScene);
            mScene->ClearUpdateFlags();
        }

        // Poll system events:
        mWindow.PollEvents();

        // Update Renderer
        mRender.OnUpdate(mDeltaTime / 1000.0f);

        // Collect imgui calls
        imutils::BeginGuiFrame();
        mSceneEditor.OnImGui(*mScene);
        mRender.OnImGui();
        imutils::FinalizeGuiFrame();

        // Render things:
        mRender.OnRender();
    }

    vkDeviceWaitIdle(mCtx.Device);
    imutils::DestroyImGui();
}

void Application::OnResize(uint32_t width, uint32_t height)
{
    mCtx.SwapchainOk = false;
    mCtx.RequestedWidth = width;
    mCtx.RequestedHeight = height;
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