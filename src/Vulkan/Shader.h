#pragma once

#include "VulkanContext.h"

#include <string>
#include <optional>

struct ReflectionInfo{
    std::optional<std::array<uint32_t, 3>> LocalSize;
    std::optional<uint32_t>                PushConstantsSize;
};

struct ShaderStageInfo{
    VkPipelineShaderStageCreateInfo PipelineCreation;
    ReflectionInfo                  Reflection;
}; 

namespace Shader{
    ShaderStageInfo GetStageInfo(VulkanContext &ctx, 
                    const std::string &path,
                    VkShaderStageFlagBits stage);
};