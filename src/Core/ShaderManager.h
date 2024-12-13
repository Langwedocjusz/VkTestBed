#pragma once

#include <filesystem>
#include <optional>

class ShaderManager {
  public:
    ShaderManager(std::string_view srcDir, std::string_view byteDir);

    bool CompilationScheduled();
    void CompileToBytecode();

    void OnImGui();

  private:
    std::optional<std::filesystem::path> GetDstPath(std::filesystem::path& src);

  private:
    std::filesystem::path mSourceDir;
    std::filesystem::path mBytecodeDir;

    bool mCompilationScheduled = false;
};