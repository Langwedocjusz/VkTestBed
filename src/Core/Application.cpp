#include "Application.h"
#include <memory>

#include "ImGuiUtils.h"
#include "Primitives.h"
#include "glm/ext/quaternion_trigonometric.hpp"

Application::Application()
    : mWindow(800, 600, "Vulkanik", static_cast<void *>(this)),
      mCtx(800, 600, "VkTestBed", mWindow), mRender(mCtx)
{
    imutils::InitImGui();
    imutils::InitGlfwBackend(mWindow.Get());
    mRender.InitImGuiVulkanBackend();

    mScene = std::make_unique<Scene>();

    mScene->Objects.emplace_back();
    mScene->Objects.back().Provider = primitive::HelloTriangle();
    {
        auto &instances = mScene->Objects.back().Instances;

        InstanceData data{};
        data.Translation = {-0.5f, 0.0f, 0.0f};
        data.Rotation = glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        instances.push_back(data);
    }

    mScene->Objects.emplace_back();
    mScene->Objects.back().Provider = primitive::HelloQuad();
    {
        auto &instances = mScene->Objects.back().Instances;

        InstanceData data{};
        data.Translation = {0.5f, 0.0f, 0.0f};
        data.Scale = {1.0f, 1.5f, 1.0f};

        instances.push_back(data);
    }

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
        mRender.OnUpdate(mDeltaTime);

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