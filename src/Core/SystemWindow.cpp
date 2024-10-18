#include "SystemWindow.h"

#include "Application.h"

#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

static void FramebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));

    app->OnResize(width, height);
}

SystemWindow::SystemWindow(uint32_t width, uint32_t height, std::string title,
                           void *usr_ptr)
{
    auto error_callback = [](int error, const char *description) {
        (void)error;
        std::cerr << "Glfw Error: " << description << '\n';
    };

    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        throw std::runtime_error("Failed to initialize glfw!");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    mWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    if (!mWindow)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create a window!");
    }

    glfwSetWindowUserPointer(mWindow, usr_ptr);
    glfwSetFramebufferSizeCallback(mWindow, FramebufferResizeCallback);
}

SystemWindow::~SystemWindow()
{
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}

bool SystemWindow::ShouldClose()
{
    return glfwWindowShouldClose(mWindow);
}

void SystemWindow::PollEvents()
{
    glfwPollEvents();
}

void SystemWindow::WaitEvents()
{
    glfwWaitEvents();
}

VkSurfaceKHR SystemWindow::CreateSurface(VkInstance instance,
                                         VkAllocationCallbacks *allocator)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult err = glfwCreateWindowSurface(instance, mWindow, allocator, &surface);

    if (err != VK_SUCCESS)
        throw std::runtime_error("Failed to create a surface!");

    return surface;
}