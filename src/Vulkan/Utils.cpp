#include "Utils.h"

#include <stdexcept>

void utils::BeginRecording(VkCommandBuffer buffer)
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(buffer, &begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin recording command buffer!");
}

void utils::EndRecording(VkCommandBuffer buffer)
{
    if (vkEndCommandBuffer(buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to record command buffer!");
}