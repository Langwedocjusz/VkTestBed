#include "Shader.h"
#include "Pch.h"

#include <libassert/assert.hpp>

#include <format>
#include <fstream>
#include <iostream>

static std::vector<char> ReadFileBinary(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    ASSERT(file, std::format("Failed to open file: {}", filename));

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(file_size));

    file.close();

    return buffer;
}

static VkShaderModule CreateShaderModule(VulkanContext &ctx,
                                         const std::vector<char> &code)
{
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    auto ret = vkCreateShaderModule(ctx.Device, &create_info, nullptr, &shaderModule);

    if (ret != VK_SUCCESS)
    {
        std::cerr << "Failed to create a shader module!\n";
        return VK_NULL_HANDLE; // failed to create shader module
    }

    return shaderModule;
}

std::vector<VkPipelineShaderStageCreateInfo> ShaderBuilder::Build(VulkanContext &ctx)
{
    if (mComputePath.has_value())
    {
        return BuildCompute(ctx);
    }

    else
    {
        ASSERT(mVertexPath.has_value(),
               "Vertex shader path not provided in non-compute shader.");

        ASSERT(mFragmentPath.has_value(),
               "Fragment shader path not provided in non-compute shader.");

        return BuildGraphics(ctx);
    }
}

std::vector<VkPipelineShaderStageCreateInfo> ShaderBuilder::BuildGraphics(
    VulkanContext &ctx)
{
    auto vertCode = ReadFileBinary(mVertexPath.value());
    auto fragCode = ReadFileBinary(mFragmentPath.value());

    VkShaderModule vertModule = CreateShaderModule(ctx, vertCode);
    ASSERT(vertModule != VK_NULL_HANDLE, "Failed to create a shader module!");

    VkShaderModule fragModule = CreateShaderModule(ctx, fragCode);
    ASSERT(fragModule != VK_NULL_HANDLE, "Failed to create a shader module!");

    VkPipelineShaderStageCreateInfo vertStageInfo = {};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertModule;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo = {};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragModule;
    fragStageInfo.pName = "main";

    return std::vector<VkPipelineShaderStageCreateInfo>{vertStageInfo, fragStageInfo};
}

std::vector<VkPipelineShaderStageCreateInfo> ShaderBuilder::BuildCompute(
    VulkanContext &ctx)
{
    auto computeCode = ReadFileBinary(mComputePath.value());

    VkShaderModule computeModule = CreateShaderModule(ctx, computeCode);
    ASSERT(computeModule != VK_NULL_HANDLE, "Failed to create a shader module!");

    VkPipelineShaderStageCreateInfo computeStageInfo = {};
    computeStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeStageInfo.module = computeModule;
    computeStageInfo.pName = "main";

    return std::vector<VkPipelineShaderStageCreateInfo>{computeStageInfo};
}