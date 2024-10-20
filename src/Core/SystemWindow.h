#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

struct GLFWwindow;

class SystemWindow {
  public:
    SystemWindow(uint32_t width, uint32_t height, std::string title,
                 void *usr_ptr = nullptr);
    ~SystemWindow();

    bool ShouldClose();

    void PollEvents();
    void WaitEvents();

    VkSurfaceKHR CreateSurface(VkInstance instance,
                               VkAllocationCallbacks *allocator = nullptr);

    //Only used to initialize imgui,
    //may think up a better way in the future
    GLFWwindow *Get()
    {
        return mWindow;
    }

  private:
    GLFWwindow *mWindow = nullptr;
};