#pragma once

#include "Event.h"

#include "volk.h"

#include <cstdint>

struct GLFWwindow;

class SystemWindow {
  public:
    using EventHandlerFn = void (*)(void *, Event::EventVariant);

  public:
    SystemWindow(uint32_t width, uint32_t height, const char *title, void *usr_ptr);
    ~SystemWindow();

    void SetEventCallback(EventHandlerFn callback);

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