#include "FileHandle.h"
#include "Pch.h"

#include "Path.h"

#include "Vassert.h"

#include <filesystem>
#include <fstream>
#include <utility>

struct FileHandle::Impl {
    std::ifstream Stream;
    int64_t       Size;
};

FileHandle::FileHandle() = default;

std::optional<FileHandle> FileHandle::TryOpen(const std::string &path)
{
    // We use std::ios::ate to automatically go to the end:
    constexpr auto flags = std::ios::binary | std::ios::ate;

    auto u8view = Utf8FromString(path);

    std::ifstream stream{};
    stream.open(std::filesystem::path(u8view), flags);

    if (!stream)
    {
        return std::nullopt;
    }

    // So we can recover file-size this way:
    auto size = static_cast<int64_t>(stream.tellg());

    // And now reset:
    stream.seekg(0, stream.beg);

    // And finally wrap in the FileHandle object:
    FileHandle ret{};

    ret.mImpl = std::make_unique<Impl>(Impl{
        .Stream = std::move(stream),
        .Size   = size,
    });

    return ret;
}

FileHandle::~FileHandle()
{
}

FileHandle::FileHandle(FileHandle &&) noexcept            = default;
FileHandle &FileHandle::operator=(FileHandle &&) noexcept = default;

bool FileHandle::Good()
{
    return static_cast<bool>(mImpl->Stream);
}

int64_t FileHandle::Size()
{
    return mImpl->Size;
}

int64_t FileHandle::TellG()
{
    return mImpl->Stream.tellg();
}

void FileHandle::SeekG(int64_t offset, Dir dir)
{
    auto rawDir = [&]() {
        switch (dir)
        {
        case Dir::Beg:
            return std::ios::beg;
        case Dir::Cur:
            return std::ios::cur;
        case Dir::End:
            return std::ios::end;
        }
        std::unreachable();
    }();

    mImpl->Stream.seekg(offset, rawDir);
}

bool FileHandle::Read(char *memory, int64_t size)
{
    vassert(size >= 0);
    vassert(size <= mImpl->Size);

    return static_cast<bool>(mImpl->Stream.read(memory, size));
}

int64_t FileHandle::GCount()
{
    return mImpl->Stream.gcount();
}

void FileHandle::Clear()
{
    mImpl->Stream.clear();
}

bool FileHandle::GetLine(std::string &line)
{
    return static_cast<bool>(std::getline(mImpl->Stream, line));
}