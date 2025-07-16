#include "Pch.h"

#include "SystemWindow.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "Assert.h"

#include <iostream>

// Static storage for actuall callback function pointers:
static SystemWindow::FramebufferCallbackPtr sFramebufferResizeCallback = nullptr;
static SystemWindow::KeyCallbackPtr sKeyCallback = nullptr;
static SystemWindow::MouseMovedCallbackPtr sMouseMovedCallback = nullptr;

// Redirection functions that redirect to user callbacks:
static void FramebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto usr_ptr = glfwGetWindowUserPointer(window);
    sFramebufferResizeCallback(usr_ptr, width, height);
}

static void KeyCallback(GLFWwindow *window, int key, int /*scancode*/, int action,
                        int /*mods*/)
{
    auto usr_ptr = glfwGetWindowUserPointer(window);
    sKeyCallback(usr_ptr, key, action);
}

static void MouseMovedCallback(GLFWwindow *window, double xPos, double yPos)
{
    auto usr_ptr = glfwGetWindowUserPointer(window);
    sMouseMovedCallback(usr_ptr, static_cast<float>(xPos), static_cast<float>(yPos));
}

SystemWindow::SystemWindow(uint32_t width, uint32_t height, std::string title,
                           void *usr_ptr)
{
    auto error_callback = [](int error, const char *description) {
        (void)error;
        std::cerr << "Glfw Error: " << description << '\n';
    };

    glfwSetErrorCallback(error_callback);

    vassert(glfwInit(), "Failed to initialize glfw!");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    mWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

    if (!mWindow)
    {
        glfwTerminate();
        vpanic("Failed to create a window!");
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

void SystemWindow::SetFramebufferResizeCallback(FramebufferCallbackPtr ptr)
{
    sFramebufferResizeCallback = ptr;
}

void SystemWindow::SetKeyCallback(KeyCallbackPtr ptr)
{
    sKeyCallback = ptr;
}

void SystemWindow::SetMouseMovedCallback(MouseMovedCallbackPtr ptr)
{
    sMouseMovedCallback = ptr;
}

VkSurfaceKHR SystemWindow::CreateSurface(VkInstance instance,
                                         VkAllocationCallbacks *allocator)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult err = glfwCreateWindowSurface(instance, mWindow, allocator, &surface);

    vassert(err == VK_SUCCESS, "Failed to create a surface!");

    return surface;
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