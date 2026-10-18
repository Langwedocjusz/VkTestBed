#include "Shader.h"
#include "Pch.h"

#include "FileHandle.h"
#include "Vassert.h"

#include "spirv_reflect.h"

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
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t); // In bytes
    createInfo.pCode    = code.data();

    VkShaderModule shaderModule;
    auto ret = vkCreateShaderModule(ctx.Device, &createInfo, nullptr, &shaderModule);

    if (ret != VK_SUCCESS)
    {
        std::cerr << "Failed to create a shader module!\n";
        return VK_NULL_HANDLE; 
    }

    return shaderModule;
}

static ReflectionInfo GetReflectionInfo(std::vector<uint32_t> &code)
{
    ReflectionInfo ret{};

    auto data = static_cast<void*>(code.data());
    auto size = sizeof(uint32_t) * code.size();

    SpvReflectShaderModule module;

    {
        auto result = spvReflectCreateShaderModule(size, data, &module);
        vassert(result == SPV_REFLECT_RESULT_SUCCESS);
    }

    // Assume entry point is always called 'main':
    auto entryPoint = spvReflectGetEntryPoint(&module, "main");
    vassert(entryPoint != nullptr);

    // Get local group sizes for compute shaders:
    if (entryPoint->shader_stage & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT)
    {
        ret.LocalSize = {{
            entryPoint->local_size.x,
            entryPoint->local_size.y,
            entryPoint->local_size.z,
        }};
    }

    // Retrieve information about push constant blocks:
    uint32_t pcBlockCount;
    {
        auto result = spvReflectEnumeratePushConstantBlocks(&module, &pcBlockCount, nullptr);
        vassert(result == SPV_REFLECT_RESULT_SUCCESS);
    }

    std::vector<SpvReflectBlockVariable*> pcBlocks{};
    pcBlocks.resize(pcBlockCount);
    {
        auto result = spvReflectEnumeratePushConstantBlocks(&module, &pcBlockCount, pcBlocks.data());
        vassert(result == SPV_REFLECT_RESULT_SUCCESS);   
    }

    // Assuming there is at most 1 block.
    // Afaik GLSL enforces this anyway.
    vassert(pcBlocks.size() < 2);

    if (pcBlocks.size() == 1)
    {
        ret.PushConstantsSize = pcBlocks[0]->size;
    }

    return ret;
}

ShaderStageInfo Shader::GetStageInfo(VulkanContext &ctx, const std::string &path, VkShaderStageFlagBits stage)
{
    auto code = ReadSpirvBinary(path);

    VkShaderModule vertModule = CreateShaderModule(ctx, code);
    vassert(vertModule != VK_NULL_HANDLE, "Failed to create a shader module!");

    VkPipelineShaderStageCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage  = stage;
    pipelineInfo.module = vertModule;
    pipelineInfo.pName  = "main";

    auto reflectionInfo = GetReflectionInfo(code);

    ShaderStageInfo ret{
        .PipelineCreation = pipelineInfo,
        .Reflection       = reflectionInfo,
    };

    return ret;
}

