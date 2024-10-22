#include "Application.h"
#include <memory>

#include "ImGuiUtils.h"
#include "Primitives.h"

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

        instances.push_back(data);
    }

    mScene->Objects.emplace_back();
    mScene->Objects.back().Provider = primitive::HelloQuad();
    {
        auto &instances = mScene->Objects.back().Instances;

        InstanceData data{};
        data.Translation = {0.5f, 0.0f, 0.0f};

        instances.push_back(data);
    }

    mRender.LoadScene(*mScene);
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

        // Poll system events:
        mWindow.PollEvents();

        // Update Renderer
        mRender.OnUpdate(mDeltaTime);

        // Collect imgui calls
        imutils::BeginGuiFrame();
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