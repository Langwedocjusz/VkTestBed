#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

struct GLFWwindow;

class SystemWindow {
  public:
    using FramebufferCallbackPtr = void (*)(void *, int, int);
    using KeyCallbackPtr = void (*)(void *, int, int);
    using MouseMovedCallbackPtr = void (*)(void *, float, float);

  public:
    SystemWindow(uint32_t width, uint32_t height, std::string title, void *usr_ptr);
    ~SystemWindow();

    void SetFramebufferResizeCallback(FramebufferCallbackPtr ptr);
    void SetKeyCallback(KeyCallbackPtr ptr);
    void SetMouseMovedCallback(MouseMovedCallbackPtr ptr);

    VkSurfaceKHR CreateSurface(VkInstance instance,
                               VkAllocationCallbacks *allocator = nullptr);

    bool ShouldClose();
    void PollEvents();
    void WaitEvents();
    void CaptureCursor();
    void FreeCursor();

    // Only used to initialize imgui,
    // may think up a better way in the future
    GLFWwindow *Get()
    {
        return mWindow;
    }

  private:
    GLFWwindow *mWindow = nullptr;
};