#include "Shader.h"
#include "Pch.h"

#include "FileHandle.h"
#include "Vassert.h"

#include <iostream>

static std::vector<uint32_t> ReadSpirvBinary(const std::string &filename)
{
    auto file = *FileHandle::TryOpen(filename);

    if (!file.Good())
    {
        vpanic("Failed to open file: " + filename);
    }

    const auto fileSize = file.Size();

    if (fileSize % sizeof(uint32_t) != 0)
    {
        vpanic("File: " + filename + " has invalid spirv. Size is " +
               std::to_string(fileSize) + "bytes. Should be divisible by 4.");
    }

    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.Read(reinterpret_cast<char *>(buffer.data()), fileSize);

    return buffer;
}

static VkShaderModule CreateShaderModule(VulkanContext               &ctx,
                                         const std::vector<uint32_t> &code)
{
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize                 = code.size() * sizeof(uint32_t); // In bytes
    create_info.pCode                    = code.data();

    VkShaderModule shaderModule;
    auto ret = vkCreateShaderModule(ctx.Device, &create_info, nullptr, &shaderModule);

    if (ret != VK_SUCCESS)
    {
        std::cerr << "Failed to create a shader module!\n";
        return VK_NULL_HANDLE; // failed to create shader module
    }

    return shaderModule;
}

VkPipelineShaderStageCreateInfo Shader::GetStageInfo(VulkanContext &ctx, const std::string &path, VkShaderStageFlagBits stage)
{
    auto vertCode = ReadSpirvBinary(path);

    VkShaderModule vertModule = CreateShaderModule(ctx, vertCode);
    vassert(vertModule != VK_NULL_HANDLE, "Failed to create a shader module!");

    VkPipelineShaderStageCreateInfo ret{};
    ret.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ret.stage  = stage;
    ret.module = vertModule;
    ret.pName  = "main";

    return ret;
}

