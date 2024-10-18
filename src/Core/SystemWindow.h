#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

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

  private:
    struct GLFWwindow *mWindow = nullptr;
};