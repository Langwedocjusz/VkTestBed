#include "Shader.h"
#include "Pch.h"

#include "Vassert.h"

#include <fstream>
#include <iostream>

static std::vector<char> ReadFileBinary(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file)
    {
        vpanic("Failed to open file: " + filename);
    }

    size_t            file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(file_size));

    file.close();

    return buffer;
}

static VkShaderModule CreateShaderModule(VulkanContext           &ctx,
                                         const std::vector<char> &code)
{
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize                 = code.size();
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
        vassert(!mVertexPath.has_value(),
                "Vertex shader path provided in a compute shader.");

        vassert(!mFragmentPath.has_value(),
                "Fragment shader path provided in a compute shader.");

        return BuildCompute(ctx);
    }

    else
    {
        return BuildGraphics(ctx);
    }
}

std::vector<VkPipelineShaderStageCreateInfo> ShaderBuilder::BuildGraphics(
    VulkanContext &ctx)
{
    std::vector<VkPipelineShaderStageCreateInfo> res{};

    if (mVertexPath.has_value())
    {
        auto vertCode = ReadFileBinary(mVertexPath.value());

        VkShaderModule vertModule = CreateShaderModule(ctx, vertCode);
        vassert(vertModule != VK_NULL_HANDLE, "Failed to create a shader module!");

        VkPipelineShaderStageCreateInfo vertStageInfo = {};
        vertStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = vertModule;
        vertStageInfo.pName  = "main";

        res.push_back(vertStageInfo);
    }

    if (mFragmentPath.has_value())
    {
        auto fragCode = ReadFileBinary(mFragmentPath.value());

        VkShaderModule fragModule = CreateShaderModule(ctx, fragCode);
        vassert(fragModule != VK_NULL_HANDLE, "Failed to create a shader module!");

        VkPipelineShaderStageCreateInfo fragStageInfo = {};
        fragStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = fragModule;
        fragStageInfo.pName  = "main";

        res.push_back(fragStageInfo);
    }

    return res;
}

std::vector<VkPipelineShaderStageCreateInfo> ShaderBuilder::BuildCompute(
    VulkanContext &ctx)
{
    auto computeCode = ReadFileBinary(mComputePath.value());

    VkShaderModule computeModule = CreateShaderModule(ctx, computeCode);
    vassert(computeModule != VK_NULL_HANDLE, "Failed to create a shader module!");

    VkPipelineShaderStageCreateInfo computeStageInfo = {};
    computeStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeStageInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    computeStageInfo.module = computeModule;
    computeStageInfo.pName  = "main";

    return std::vector<VkPipelineShaderStageCreateInfo>{computeStageInfo};
}