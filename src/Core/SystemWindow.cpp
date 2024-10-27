#include "SystemWindow.h"

#include "Application.h"

#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan.h>

#include "Event.h"
#include <GLFW/glfw3.h>

static void FramebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));

    app->OnResize(width, height);
}

static void KeyCallback(GLFWwindow *window, int key, int /*scancode*/, int action,
                        int /*mods*/)
{
    auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));

    switch (action)
    {
    case GLFW_PRESS: {
        app->OnEvent(Event::KeyPressed(key, false));
        break;
    }
    case GLFW_REPEAT: {
        app->OnEvent(Event::KeyPressed(key, true));
        break;
    }
    case GLFW_RELEASE: {
        app->OnEvent(Event::KeyReleased(key));
        break;
    }
    }
}

static void MouseMovedCallback(GLFWwindow *window, double xPos, double yPos)
{
    auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));

    app->OnEvent(Event::MouseMoved(static_cast<float>(xPos), static_cast<float>(yPos)));
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

    glfwSetKeyCallback(mWindow, KeyCallback);
    glfwSetCursorPosCallback(mWindow, MouseMovedCallback);
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

void SystemWindow::CaptureCursor()
{
    glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void SystemWindow::FreeCursor()
{
    glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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