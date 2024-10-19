#include "Application.h"
#include <memory>

Application::Application()
    : mWindow(800, 600, "Vulkanik", static_cast<void *>(this)),
      mCtx(800, 600, "VkTestBed", mWindow), mRender(mCtx)
{
    mScene = std::make_unique<Scene>();
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

        // Render things:
        mRender.OnUpdate(mDeltaTime);
        mRender.OnRender();
    }

    vkDeviceWaitIdle(mCtx.Device);
}

void Application::OnResize(uint32_t width, uint32_t height)
{
    mCtx.SwapchainOk = false;
    mCtx.RequestedWidth = width;
    mCtx.RequestedHeight = height;
}