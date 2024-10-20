#pragma once

#include "VulkanContext.h"

struct GLFWwindow;

namespace imutils
{
void InitImGui();
void InitGlfwBackend(GLFWwindow *window);
VkDescriptorPool CreateDescriptorPool(VulkanContext &ctx);
void InitVulkanBackend(VulkanContext &ctx, VkDescriptorPool descriptorPool,
                       VkQueue graphicsQueue, uint32_t framesInFlight);

void BeginGuiFrame();
void FinalizeGuiFrame();
void RecordImguiToCommandBuffer(VkCommandBuffer cmd);

void DestroyImGui();
} // namespace imutils