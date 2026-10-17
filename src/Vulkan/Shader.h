#pragma once

#include "VulkanContext.h"

#include <string>

struct ReflectionInfo{
    uint32_t LocalSizeX;
    uint32_t LocalSizeY;
    uint32_t LocalSizeZ;
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