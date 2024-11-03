#include "Application.h"
#include <memory>

#include "Event.h"
#include "ImGuiInit.h"
#include "Keycodes.h"
#include "ModelLoader.h"
#include "Primitives.h"

#include <iostream>
#include <variant>

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

Application::Application()
    : mWindow(800, 600, "Vulkanik", static_cast<void *>(this)),
      mCtx(800, 600, "VkTestBed", mWindow), mRender(mCtx)
{
    iminit::InitImGui();
    iminit::InitGlfwBackend(mWindow.Get());
    mRender.InitImGuiVulkanBackend();

    mScene = std::make_unique<Scene>();

    mScene->Objects.push_back(SceneObject{
        .Provider = primitive::ColoredCube(glm::vec3(0.5f)),
        .TextureId = {},
        .Instances = {InstanceData{}},
    });

    size_t texId = mScene->Textures.insert("./assets/textures/texture.jpg");

    mScene->Objects.push_back(SceneObject{
        .Provider = primitive::TexturedCube(),
        .TextureId = texId,
        .Instances = {InstanceData{
            .Translation = {1.5f, 0.0f, 0.0f},
            .Rotation = glm::vec3(0.0f),
            .Scale = glm::vec3(1.0f),
        }},
    });

    texId = mScene->Textures.insert("./assets/gltf/DamagedHelmet/Default_albedo.jpg");

    mScene->Objects.push_back(SceneObject{
        .Provider = ModelLoader::PosTex("assets/gltf/DamagedHelmet/DamagedHelmet.gltf"),
        .TextureId = texId,
        .Instances = {InstanceData{
            .Translation = {3.0f, 0.0f, 0.0f},
            .Rotation = {-0.5f * 3.14f, 0.0f, 0.0f},
            .Scale = glm::vec3(1.0f),
        }},
    });

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
        iminit::BeginGuiFrame();
        mSceneEditor.OnImGui(*mScene);
        mRender.OnImGui();
        iminit::FinalizeGuiFrame();

        // Render things:
        mRender.OnRender();
    }

    vkDeviceWaitIdle(mCtx.Device);
    iminit::DestroyImGui();
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