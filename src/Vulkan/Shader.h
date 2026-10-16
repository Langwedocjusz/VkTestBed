#pragma once

#include "VulkanContext.h"

//#include <optional>
#include <string>

namespace Shader{
    VkPipelineShaderStageCreateInfo GetStageInfo(VulkanContext &ctx, 
                                                 const std::string &path,
                                                 VkShaderStageFlagBits stage);
};