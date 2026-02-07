#pragma once

#include <filesystem>
#include <optional>

namespace efsw
{
class FileWatcher;
}

class UpdateListener;

class ShaderManager {
  public:
    ShaderManager(std::string_view srcDir, std::string_view byteDir);

    bool CompilationScheduled();
    void CompileToBytecode();

  private:
    std::optional<std::filesystem::path> GetDstPath(std::filesystem::path &src);

  private:
    std::filesystem::path mSourceDir;
    std::filesystem::path mBytecodeDir;

    bool mCompilationScheduled = false;

    efsw::FileWatcher *mFileWatcher;
    UpdateListener    *mUpdateListener;
};