#include "ShaderManager.h"
#include "Pch.h"

#include "Path.h"

#include <efsw/efsw.hpp>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <ranges>
#include <regex>
#include <set>
#include <vector>

class UpdateListener : public efsw::FileWatchListener {
  public:
    UpdateListener([[maybe_unused]] std::function<void()> callback) : mCallback(std::move(callback))
    {
    }

    void handleFileAction(efsw::WatchID watchid, const std::string &dir,
                          const std::string &filename, efsw::Action action,
                          std::string oldFilename) override
    {
        (void)watchid;
        (void)dir;
        (void)filename;
        (void)action;
        (void)oldFilename;
        
        switch (action)
        {
        case efsw::Actions::Modified:
            mCallback();
            break;
        default:
            break;
        }
    }

  private:
    std::function<void()> mCallback;
};

ShaderManager::ShaderManager(const std::string &srcDir, const std::string &byteDir)
{
    mSourceDir   = Path::Current() / Path(srcDir);
    mBytecodeDir = Path::Current() / Path(byteDir);

    // Create bytecode dir if it doesn't already exist:
    mBytecodeDir.CreateDirectory();

    for (auto subdir: RecursiveDirectoryRange(mSourceDir)) 
    {
        if (!subdir.IsDirectory())
            continue;

        auto relative = subdir.Relative(mSourceDir);

        auto rebased = mBytecodeDir / relative;
        rebased.CreateDirectory();
    }

    CompileToBytecode();

    // Setup directory watcher:
    mFileWatcher    = std::make_unique<efsw::FileWatcher>();
    mUpdateListener = std::make_unique<UpdateListener>([this]() { mCompilationScheduled = true; });

    mFileWatcher->addWatch(srcDir, mUpdateListener.get(), true);
    mFileWatcher->watch();
}

ShaderManager::~ShaderManager() = default;

bool ShaderManager::CompilationScheduled()
{
    return mCompilationScheduled;
}

std::optional<Path> ShaderManager::GetDstPath(Path &src)
{
    auto parentPath    = src.Parent();
    auto relParentPath = parentPath.Relative(mSourceDir);

    auto extension = src.Extension();
    std::string filename = src.Stem();

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

static std::string GetFilename(const std::string &includeLine)
{
    const auto first = includeLine.find_first_of('\"');
    const auto last  = includeLine.find_last_of('\"');

    return includeLine.substr(first + 1, last - first - 1);
}

static std::vector<size_t> GetIncludedFileIds(
    Path &srcDir, const std::vector<Path> &fileList,
    size_t id)
{
    std::vector<size_t> res;

    const auto &path = fileList.at(id);
    auto file        = path.Open();

    const std::regex incRegex("[[:blank:]]*#[[:blank:]]*include[[:blank:]]+\".*\"");

    std::string currentLine;
    while (std::getline(file, currentLine))
    {
        if (std::regex_match(currentLine, incRegex))
        {
            auto filepath = srcDir / GetFilename(currentLine);

            auto iter = std::ranges::find(fileList, filepath);

            size_t index = std::distance(fileList.begin(), iter);

            if (index != fileList.size())
            {
                res.push_back(index);
            }
        }
    }

    return res;
}

static std::vector<std::vector<size_t>> GetAdjacencyList(
    Path &srcDir, const std::vector<Path> &fileList)
{
    const size_t numFiles = fileList.size();

    std::vector<std::vector<size_t>> res(numFiles);

    for (size_t i = 0; i < numFiles; i++)
    {
        res[i] = GetIncludedFileIds(srcDir, fileList, i);
    }

    return res;
}

void ShaderManager::CompileToBytecode()
{
    using namespace std::views;

    mCompilationScheduled = false;

    // Retrieve shader source file list:
    std::vector<Path> fileList;

    for (auto&& dir : RecursiveDirectoryRange(mSourceDir))
    {
        if (dir.IsRegularFile())
        {
            fileList.emplace_back(dir);
        }
    }

    // Construct adjacency list out of it, based on the presence of
    // include directives:
    auto adjacencyList = GetAdjacencyList(mSourceDir, fileList);

    // TODO: Check if graph represented by the adjacency list is acyclic

    // Reverse the adjacency list:
    std::vector<std::vector<size_t>> reverseList(adjacencyList.size());

    for (size_t i = 0; i < adjacencyList.size(); i++)
    {
        for (auto elem : adjacencyList[i])
            reverseList[elem].push_back(i);
    }

    // Assume that files that are not included anywhere are the ones
    // to be compiled:

    std::set<size_t> nonHeaderIds;

    for (auto [idx, sublist] : enumerate(reverseList))
    {
        if (sublist.size() == 0)
            nonHeaderIds.insert(idx);
    }

    // Prune the set of compilable files based on
    // wether or not they or their included fles
    // have been updated since last run:

    struct CompilerArgs {
        Path Src;
        Path Dst;
    };

    std::vector<CompilerArgs> data;

    for (auto id : nonHeaderIds)
    {
        auto srcPath = fileList.at(id);

        auto dstPathOpt = GetDstPath(srcPath);

        if (!dstPathOpt.has_value())
            continue;

        Path &dstPath = *dstPathOpt;

        // If dst exists and is newer than src
        // there is no need to call the compiler.
        bool alreadyExists = dstPath.Exists();

        if (alreadyExists)
        {
            auto dstTime = dstPath.LastWriteTime();
            auto srcTime = srcPath.LastWriteTime();

            // TODO: this currently only supporst 1-long include chains
            // It should really traverse the whole include DAG.
            for (auto headerId : adjacencyList[id])
            {
                auto headerTime = fileList[headerId].LastWriteTime();

                srcTime = std::max(srcTime, headerTime);
            }

            if (srcTime < dstTime)
                continue;
        }

        // Append compiler call arguments:
        data.push_back(CompilerArgs{
            .Src = srcPath,
            .Dst = dstPath,
        });
    }

    // Call glslc compiler with all collected arguments:

    for (const auto &args : data)
    {
        // TODO: Check if string encoding is ok for system:
        auto srcDir = args.Src.String();
        auto dstDir = args.Dst.String();

        std::string cmd = "glslc --target-env=vulkan1.3 " + srcDir + " -o " + dstDir;

        // TODO: maybe figure out a nicer way to do this:
        system(cmd.c_str());
    }
}