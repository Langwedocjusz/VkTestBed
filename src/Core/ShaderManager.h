#pragma once

#include "Path.h"

#include <memory>
#include <optional>

namespace efsw
{
class FileWatcher;
}

class UpdateListener;

class ShaderManager {
  public:
    ShaderManager(const std::string &srcDir, const std::string &byteDir);
    ~ShaderManager();

    bool CompilationScheduled();
    void CompileToBytecode();

  private:
    std::optional<Path> GetDstPath(Path &src);

    Path mSourceDir;
    Path mBytecodeDir;

    bool mCompilationScheduled = false;

    std::unique_ptr<efsw::FileWatcher> mFileWatcher;
    std::unique_ptr<UpdateListener>    mUpdateListener;
};