#include "ShaderManager.h"
#include "Pch.h"

#include <algorithm>
#include <efsw/efsw.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ranges>
#include <regex>
#include <set>
#include <vector>

class UpdateListener : public efsw::FileWatchListener {
  public:
    UpdateListener(std::function<void()> callback) : mCallback(std::move(callback))
    {
    }

    void handleFileAction(efsw::WatchID watchid, const std::string &dir,
                          const std::string &filename, efsw::Action action,
                          std::string oldFilename) override
    {
        (void)watchid;
        (void)dir;
        (void)filename;
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

ShaderManager::ShaderManager(std::string_view srcDir, std::string_view byteDir)
{
    mSourceDir = std::filesystem::current_path() / srcDir;
    mBytecodeDir = std::filesystem::current_path() / byteDir;

    // Create bytecode dir if it doesn't already exist:
    std::filesystem::create_directory(mBytecodeDir);

    CompileToBytecode();

    // Setup directory watcher:
    mFileWatcher = new efsw::FileWatcher();
    mUpdateListener = new UpdateListener([this]() { mCompilationScheduled = true; });

    mFileWatcher->addWatch(mSourceDir.string(), mUpdateListener, true);
    mFileWatcher->watch();
}

bool ShaderManager::CompilationScheduled()
{
    return mCompilationScheduled;
}

std::optional<std::filesystem::path> ShaderManager::GetDstPath(std::filesystem::path &src)
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

static std::string GetFilename(const std::string &includeLine)
{
    const auto first = includeLine.find_first_of('\"');
    const auto last = includeLine.find_last_of('\"');

    return includeLine.substr(first + 1, last - first - 1);
}

static std::vector<size_t> GetIncludedFileIds(
    std::filesystem::path &srcDir, const std::vector<std::filesystem::path> &fileList,
    size_t id)
{
    std::vector<size_t> res;

    const auto &path = fileList.at(id);
    std::ifstream file(path);

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
    std::filesystem::path &srcDir, const std::vector<std::filesystem::path> &fileList)
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
    std::vector<std::filesystem::path> fileList;

    for (const auto &dir : std::filesystem::recursive_directory_iterator(mSourceDir))
    {
        if (std::filesystem::is_regular_file(dir.path()))
        {
            fileList.emplace_back(dir.path());
        }
    }

    // Construct adjacency list out of it, based on the presence of
    // include directives:
    auto adjacencyList = GetAdjacencyList(mSourceDir, fileList);

    // To-do: Check if graph represented by the adjacency list is acyclic

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
        std::filesystem::path Src;
        std::filesystem::path Dst;
    };

    std::vector<CompilerArgs> data;

    for (auto id : nonHeaderIds)
    {
        auto srcPath = fileList.at(id);
        auto dstPathOpt = GetDstPath(srcPath);

        if (!dstPathOpt.has_value())
            continue;

        auto dstPath = dstPathOpt.value();

        // If dst exists and is newer than src
        // there is no need to call the compiler.
        bool alreadyExists = std::filesystem::exists(dstPath);

        if (alreadyExists)
        {
            auto dstTime = std::filesystem::last_write_time(dstPath);
            auto srcTime = std::filesystem::last_write_time(srcPath);

            // To-do: this currently only supporst 1-long include chains
            // It should really traverse the whole include DAG.
            for (auto headerId : adjacencyList[id])
            {
                auto headerTime = std::filesystem::last_write_time(fileList[headerId]);

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
        auto srcDir = args.Src.string();
        auto dstDir = args.Dst.string();

        std::string cmd = "glslc --target-env=vulkan1.3 " + srcDir + " -o " + dstDir;

        // To-do: maybe figure out a nicer way to do this:
        system(cmd.c_str());
    }
}