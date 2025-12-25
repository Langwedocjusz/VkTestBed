#include "SystemWindow.h"
#include "Pch.h"

#include "Vassert.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <iostream>

static SystemWindow::EventHandlerFn sEventCallback = nullptr;

// Redirection functions that forward to user callback:
static void FramebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto usrPtr = glfwGetWindowUserPointer(window);
    auto event = Event::FramebufferResize{.Width = width, .Height = height};

    sEventCallback(usrPtr, event);
}

static void FocusCallback(GLFWwindow *window, int focused)
{
    auto usrPtr = glfwGetWindowUserPointer(window);
    auto event = Event::Focus{.Focused = focused};

    sEventCallback(usrPtr, event);
}

static void CursorEnterCallback(GLFWwindow *window, int entered)
{
    auto usrPtr = glfwGetWindowUserPointer(window);
    auto event = Event::CursorEnter{.Entered = entered};

    sEventCallback(usrPtr, event);
}

static void CursorPosCallback(GLFWwindow *window, double xpos, double ypos)
{
    auto usrPtr = glfwGetWindowUserPointer(window);
    auto event = Event::CursorPos{.XPos = xpos, .YPos = ypos};

    sEventCallback(usrPtr, event);
}

static void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    auto usrPtr = glfwGetWindowUserPointer(window);
    auto event = Event::MouseButton{.Button = button, .Action = action, .Mods = mods};

    sEventCallback(usrPtr, event);
}

static void ScrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    auto usrPtr = glfwGetWindowUserPointer(window);
    auto event = Event::Scroll{.XOffset = xoffset, .YOffset = yoffset};

    sEventCallback(usrPtr, event);
}

static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    auto usrPtr = glfwGetWindowUserPointer(window);
    auto event =
        Event::Key{.Keycode = key, .Scancode = scancode, .Action = action, .Mods = mods};

    sEventCallback(usrPtr, event);
}

static void CharCallback(GLFWwindow *window, unsigned int codepoint)
{
    auto usrPtr = glfwGetWindowUserPointer(window);
    auto event = Event::Char{.Codepoint = codepoint};

    sEventCallback(usrPtr, event);
}

SystemWindow::SystemWindow(uint32_t width, uint32_t height, const char *title,
                           void *usrPtr)
{
    auto ErrorCallback = [](int error, const char *description) {
        (void)error;
        std::cerr << "Glfw Error: " << description << '\n';
    };
    glfwSetErrorCallback(ErrorCallback);

    vassert(glfwInit(), "Failed to initialize glfw!");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    mWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);

    if (!mWindow)
    {
        glfwTerminate();
        vpanic("Failed to create a window!");
    }

    glfwSetWindowUserPointer(mWindow, usrPtr);

    glfwSetFramebufferSizeCallback(mWindow, FramebufferResizeCallback);
    glfwSetWindowFocusCallback(mWindow, FocusCallback);
    glfwSetCursorEnterCallback(mWindow, CursorEnterCallback);
    glfwSetCursorPosCallback(mWindow, CursorPosCallback);
    glfwSetMouseButtonCallback(mWindow, MouseButtonCallback);
    glfwSetScrollCallback(mWindow, ScrollCallback);
    glfwSetKeyCallback(mWindow, KeyCallback);
    glfwSetCharCallback(mWindow, CharCallback);

    // Monitor callback currenly not supported:
    // glfwSetMonitorCallback(callback);
}

SystemWindow::~SystemWindow()
{
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}

void SystemWindow::SetEventCallback(EventHandlerFn callback)
{
    sEventCallback = callback;
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