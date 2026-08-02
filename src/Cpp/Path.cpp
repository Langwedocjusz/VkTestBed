#include "Path.h"
#include "Pch.h"

#include <filesystem>

struct Path::Impl {
    std::filesystem::path Path;
};

Path Path::Current()
{
    Path ret{};
    ret.mImpl->Path = std::filesystem::current_path();

    return ret;
}

Path::Path() : mImpl(std::make_unique<Impl>())
{
}

Path::Path(const std::string &pathStr) : mImpl(std::make_unique<Impl>())
{
    std::u8string_view u8view{reinterpret_cast<const char8_t*>(pathStr.data()), pathStr.size()};
    mImpl->Path = std::filesystem::path(u8view);
}

Path::~Path()
{
}

Path::Path(Path &&) noexcept = default;
Path &Path::operator=(Path &&) noexcept = default;

static std::string U8StringToCharString(const std::u8string &u8str)
{
    return {reinterpret_cast<const char *>(u8str.data()), u8str.size()};
}

std::u8string Path::U8String() const
{
    return mImpl->Path.u8string();
}

std::string Path::String() const
{
    auto u8str = mImpl->Path.u8string();
    return {reinterpret_cast<const char*>(u8str.c_str()), u8str.size()};
}

std::string Path::Filename() const
{
    auto u8Fileanme = mImpl->Path.stem().u8string();
    return U8StringToCharString(u8Fileanme);
}

std::string Path::Stem() const
{
    auto u8Stem = mImpl->Path.stem().u8string();
    return U8StringToCharString(u8Stem);
}

std::string Path::Extension() const
{
    auto u8Ext = mImpl->Path.extension().u8string();
    return U8StringToCharString(u8Ext);
}

Path Path::Parent() const
{
    Path ret{};
    ret.mImpl->Path = mImpl->Path.parent_path();

    return ret;
}

Path Path::Relative(const Path& other) const
{
    Path ret{};
    ret.mImpl->Path = std::filesystem::relative(mImpl->Path, other.mImpl->Path);

    return ret;
}

bool Path::Exists() const
{
    return std::filesystem::exists(mImpl->Path);
}

bool Path::IsRegularFile() const
{
    return std::filesystem::is_regular_file(mImpl->Path);
}

bool Path::IsDirectory() const
{
    return std::filesystem::is_directory(mImpl->Path);
}

void Path::CreateDirectory() const
{
    std::filesystem::create_directory(mImpl->Path);
}

Path operator/(const Path &lhs, const Path&rhs)
{
    Path ret{};
    ret.mImpl->Path = lhs.mImpl->Path / rhs.mImpl->Path;

    return ret;
}

// === Implementation of the Directory Iterator: =======================================

struct DirectoryIterator::Impl {
    std::filesystem::directory_iterator Iter;
};

DirectoryIterator::DirectoryIterator() : mImpl(nullptr)
{
}

DirectoryIterator::DirectoryIterator(const Path &path) : mImpl(std::make_unique<Impl>())
{
    mImpl->Iter = std::filesystem::directory_iterator(path.mImpl->Path);
}

DirectoryIterator::~DirectoryIterator()
{
}

DirectoryIterator::DirectoryIterator(DirectoryIterator &&) noexcept            = default;
DirectoryIterator &DirectoryIterator::operator=(DirectoryIterator &&) noexcept = default;

Path DirectoryIterator::operator*() const
{
    Path ret{};
    ret.mImpl->Path = mImpl->Iter->path();

    return ret;
}

DirectoryIterator &DirectoryIterator::operator++()
{
    if (!mImpl)
        return *this;

    std::error_code errorCode;
    mImpl->Iter.increment(errorCode);

    bool pastEnd = mImpl->Iter == std::filesystem::directory_iterator{};

    if (errorCode || pastEnd)
    {
        // Reset internal state. We do this because we use
        // nullptr pimpl as end of  iteration:
        mImpl.reset();
    }

    return *this;
}

bool DirectoryIterator::operator!=(const DirectoryIterator &other) const
{
    return mImpl != other.mImpl;
}