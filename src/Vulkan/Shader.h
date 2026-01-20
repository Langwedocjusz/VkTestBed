#pragma once

#include "VulkanContext.h"

#include <optional>
#include <string>

class ShaderBuilder {
  public:
    ShaderBuilder() = default;

    ShaderBuilder &SetVertexPath(std::optional<std::string> path)
    {
        mVertexPath = path;
        return *this;
    }
    ShaderBuilder &SetFragmentPath(std::optional<std::string> path)
    {
        mFragmentPath = path;
        return *this;
    }

    ShaderBuilder &SetComputePath(std::string_view path)
    {
        mComputePath = path;
        return *this;
    }

    std::vector<VkPipelineShaderStageCreateInfo> Build(VulkanContext &ctx);

  private:
    std::vector<VkPipelineShaderStageCreateInfo> BuildGraphics(VulkanContext &ctx);
    std::vector<VkPipelineShaderStageCreateInfo> BuildCompute(VulkanContext &ctx);

  private:
    std::optional<std::string> mVertexPath;
    std::optional<std::string> mFragmentPath;

    std::optional<std::string> mComputePath;
};