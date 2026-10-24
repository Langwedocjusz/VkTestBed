#pragma once

#include "VulkanContext.h"

#include <optional>
#include <string>

struct BindingInfo {
    uint32_t         Set;
    uint32_t         Binding;
    VkDescriptorType Type;
    uint32_t         Count;
};

struct ReflectionInfo {
    VkShaderStageFlagBits                  Stage;
    std::vector<BindingInfo>               Bindings;
    std::optional<uint32_t>                PushConstantsSize;
    std::optional<std::array<uint32_t, 3>> LocalSize;
};

struct ShaderStageInfo {
    VkPipelineShaderStageCreateInfo PipelineCreation;
    ReflectionInfo                  Reflection;
};

namespace Shader
{
ShaderStageInfo GetStageInfo(VulkanContext &ctx, const std::string &path, VkShaderStageFlagBits stage);
};