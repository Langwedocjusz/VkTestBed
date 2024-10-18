#pragma once

#include <optional>
#include <string>

#include "VulkanContext.h"

class ShaderBuilder {
  public:
    ShaderBuilder() = default;

    ShaderBuilder SetVertexPath(std::string_view path)
    {
        mVertexPath = path;
        return *this;
    }
    ShaderBuilder SetFragmentPath(std::string_view path)
    {
        mFragmentPath = path;
        return *this;
    }

    ShaderBuilder SetComputePath(std::string_view path)
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