#pragma once

#include "Path.h"

#include <optional>

namespace efsw
{
class FileWatcher;
}

class UpdateListener;

class ShaderManager {
  public:
    ShaderManager(const std::string &srcDir, const std::string &byteDir);

    bool CompilationScheduled();
    void CompileToBytecode();

  private:
    std::optional<Path> GetDstPath(Path &src);

  private:
    Path mSourceDir;
    Path mBytecodeDir;

    bool mCompilationScheduled = false;

    efsw::FileWatcher *mFileWatcher;
    UpdateListener    *mUpdateListener;
};