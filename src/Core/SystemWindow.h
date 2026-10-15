#pragma once

#include "Event.h"

#include "volk.h"

#include <cstdint>
#include <string>

struct GLFWwindow;

#ifdef VULKAN_ON_DXGI
struct HWND__;
typedef struct HWND__ *HWND;
#endif

class SystemWindow {
  public:
    using EventHandlerFn = void (*)(void *, Event::EventVariant);

  public:
    SystemWindow(uint32_t width, uint32_t height, const std::string &title,
                 void *usr_ptr);
    ~SystemWindow();

    void SetEventCallback(EventHandlerFn callback);

    VkSurfaceKHR CreateSurface(VkInstance             instance,
                               VkAllocationCallbacks *allocator = nullptr);

    bool ShouldClose();
    void PollEvents();
    void WaitEvents();
    void CaptureCursor();
    void FreeCursor();

    void GetWindowSize(int32_t &width, int32_t &height);

    // Only used to initialize imgui,
    // may think up a better way in the future
    GLFWwindow *Get()
    {
        return mWindow;
    }

// Only used to initialize dxgi on windows.
// Same as above:
#ifdef VULKAN_ON_DXGI
    HWND GetNativeHandle();
#endif
  public:
    const std::string Title;

  private:
    GLFWwindow *mWindow = nullptr;
};