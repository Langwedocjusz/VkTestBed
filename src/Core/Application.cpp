#include "Application.h"
#include <memory>

#include "Event.h"
#include "ImGuiInit.h"
#include "Keycodes.h"
#include "ModelLoader.h"
#include "Primitives.h"
#include "imgui.h"

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

    // mScene->GeoProviders.push_back(primitive::ColoredCube(glm::vec3(0.5f)));
    // mScene->GeoProviders.push_back(primitive::TexturedCube());

    {
        auto &mesh = mScene->Meshes.emplace_back();
        mesh.Name = "Colored Cube";
        mesh.GeoProvider = primitive::ColoredCube(glm::vec3(0.5f));
    }

    {
        auto &mesh = mScene->Meshes.emplace_back();
        mesh.Name = "Textured Cube";
        mesh.GeoProvider = primitive::TexturedCube();
    }

    {
        auto &mat = mScene->Materials.emplace_back();

        mat.Name = "Test Material";

        mat[Material::Albedo] = Material::ImageSource{
            .Path = "./assets/textures/texture.jpg",
            .Channel = Material::ImageChannel::RGB,
        };
    }

    // mScene->GeoProviders.push_back(
    //     ModelLoader::PosTex("assets/gltf/DamagedHelmet/DamagedHelmet.gltf"));
    {
        auto &mat = mScene->Materials.emplace_back();

        mat.Name = "Damaged Helmet";

        mat[Material::Albedo] = Material::ImageSource{
            .Path = "./assets/gltf/DamagedHelmet/Default_albedo.jpg",
            .Channel = Material::ImageChannel::RGB,
        };
    }

    // mScene->GeoProviders.push_back(ModelLoader::PosTex("assets/gltf/Sponza/Sponza.gltf"));

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