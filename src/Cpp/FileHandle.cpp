#include "FileHandle.h"
#include "Pch.h"

#include "Vassert.h"

#include <filesystem>
#include <fstream>
#include <utility>

struct FileHandle::Impl {
    std::ifstream Stream;
    int64_t       Size;
};

FileHandle::FileHandle(const std::string &path) : mImpl(std::make_unique<Impl>())
{
    // We use std::ios::ate to automatically go to the end:
    constexpr auto flags = std::ios::binary | std::ios::ate;

    std::u8string_view u8view{reinterpret_cast<const char8_t*>(path.data()), path.size()};
    mImpl->Stream.open(std::filesystem::path(u8view), flags);

    if (!mImpl->Stream)
    {
        auto msg = "Failed to open file: " + path;
        vpanic(msg);
    }

    // So we can recover file-size this way:
    mImpl->Size = static_cast<int64_t>(mImpl->Stream.tellg());

    // And now reset:
    mImpl->Stream.seekg(0, mImpl->Stream.beg);
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

void FileHandle::Read(char *memory, int64_t size)
{
    vassert(size >= 0);
    vassert(size <= mImpl->Size);

    mImpl->Stream.read(memory, size);
}

int64_t FileHandle::GCount()
{
    return mImpl->Stream.gcount();
}

void FileHandle::Clear()
{
    mImpl->Stream.clear();
}
