#include "ShaderManager.h"

#include <cstdlib>
#include <filesystem>
#include <vector>
#include <format>

#include "imgui.h"

ShaderManager::ShaderManager(std::string_view srcDir, std::string_view byteDir)
{
    mSourceDir = std::filesystem::current_path() / srcDir;
    mBytecodeDir = std::filesystem::current_path() / byteDir;

    // Create bytecode dir if it doesn't already exist:
    std::filesystem::create_directory(mBytecodeDir);

    CompileToBytecode();
}

bool ShaderManager::CompilationScheduled()
{
    return mCompilationScheduled;
}

void ShaderManager::OnImGui()
{
    ImGui::Begin("Shader Manager");

    if (ImGui::Button("Force reload"))
        mCompilationScheduled = true;

    ImGui::End();
}

std::optional<std::filesystem::path> ShaderManager::GetDstPath(std::filesystem::path& src)
{
    auto parentPath = src.parent_path();
    auto relParentPath = std::filesystem::relative(parentPath, mSourceDir);

    auto extension = src.extension();

    std::string filename = src.stem().string();

    if (extension == ".vert")
        filename += "Vert.spv";
    else if (extension == ".frag")
        filename += "Frag.spv";
    else if (extension == ".comp")
        filename += "Comp.spv";
    else
        return std::nullopt;

    return mBytecodeDir / relParentPath / filename;
}

void ShaderManager::CompileToBytecode()
{
    mCompilationScheduled = false;

    struct CompilerArgs{
        std::filesystem::path Src;
        std::filesystem::path Dst;
    };

    std::vector<CompilerArgs> data;

    for (const auto &dir : std::filesystem::recursive_directory_iterator(mSourceDir))
    {
        if (!std::filesystem::is_regular_file(dir.path()))
            continue;

        auto srcPath = dir.path();
        auto dstPathOpt = GetDstPath(srcPath);

        if (!dstPathOpt.has_value())
            continue;

        auto dstPath = dstPathOpt.value();

        //If dst exists and is newer than src
        //there is no need to call the compiler.
        bool alreadyExists = std::filesystem::exists(dstPath);

        if (alreadyExists)
        {
            auto srcTime = std::filesystem::last_write_time(srcPath);
            auto dstTime = std::filesystem::last_write_time(dstPath);

            if (srcTime < dstTime)
                continue;
        }

        //Append compiler call arguments:
        data.push_back(CompilerArgs{
            .Src = srcPath,
            .Dst = dstPath,
        });
    }

    for (const auto& args : data)
    {
        auto srcDir = args.Src.string();
        auto dstDir = args.Dst.string();

        std::string cmd = std::format("glslc {} -o {}", srcDir, dstDir);

        //To-do: maybe figure out a nicer way to do this:
        system(cmd.c_str());
    }
}