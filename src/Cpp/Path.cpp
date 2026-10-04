#include "Path.h"
#include "Pch.h"

#include <chrono>
#include <filesystem>

// NOTE: This is technically UB, as casting char* to char8_t* 
// violates strict aliasing rules:
std::u8string_view Utf8FromString(const std::string& str)
{
    return {reinterpret_cast<const char8_t*>(str.data()), str.size()};
}

// NOTE: But this one is fine as char* can point to anything:
std::string_view CharFromUtf8String(const std::u8string &u8str)
{
    return {reinterpret_cast<const char *>(u8str.data()), u8str.size()};
}

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
    auto u8view = Utf8FromString(pathStr);
    mImpl->Path = std::filesystem::path(u8view);
}

Path::~Path()
{
}

Path::Path(const Path &other) 
    : mImpl(std::make_unique<Impl>())
{
    mImpl->Path = other.mImpl->Path;
}

Path &Path::operator=(const Path &other) 
{
    if (this == &other)
        return *this;

    if (other.mImpl)
    {
        mImpl = std::make_unique<Impl>(*other.mImpl);
    }
    else
    {
        mImpl.reset();
    }

    return *this;
}

Path::Path(Path &&) noexcept = default;
Path &Path::operator=(Path &&) noexcept = default;

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
    return std::string(CharFromUtf8String(u8Fileanme));
}

std::string Path::Stem() const
{
    auto u8Stem = mImpl->Path.stem().u8string();
    return std::string(CharFromUtf8String(u8Stem));
}

std::string Path::Extension() const
{
    auto u8Ext = mImpl->Path.extension().u8string();
    return std::string(CharFromUtf8String(u8Ext));
}

Path Path::Parent() const
{
    Path ret{};
    ret.mImpl->Path = mImpl->Path.parent_path();

    return ret;
}

Path Path::Relative(const Path& base) const
{
    Path ret{};
    ret.mImpl->Path = std::filesystem::relative(mImpl->Path, base.mImpl->Path);

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

int64_t Path::LastWriteTime() const
{
    using namespace std::chrono;

    auto tp = std::filesystem::last_write_time(mImpl->Path);
    auto sys = clock_cast<system_clock>(tp);
    return duration_cast<microseconds>(sys.time_since_epoch()).count();
}

void Path::CreateDirectory() const
{
    std::filesystem::create_directory(mImpl->Path);
}

std::ifstream Path::Open() const
{
    std::ifstream ret{ mImpl->Path };
    return ret;
}

Path operator/(const Path &lhs, const Path&rhs)
{
    Path ret{};
    ret.mImpl->Path = lhs.mImpl->Path / rhs.mImpl->Path;

    return ret;
}

bool operator==(const Path &lhs, const Path &rhs)
{
    return lhs.mImpl == rhs.mImpl;
}


// === Implementation of the Directory Iterator: =======================================
// TODO: Implementations of recursive and non-recursive variants are blatant copies.
// Maybe common logic can be factored out in a non-horrible way?

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



// === Implementation of the Recursive Directory Iterator: =======================================

struct RecursiveDirectoryIterator::Impl {
    std::filesystem::recursive_directory_iterator Iter;
};

RecursiveDirectoryIterator::RecursiveDirectoryIterator() : mImpl(nullptr)
{
}

RecursiveDirectoryIterator::RecursiveDirectoryIterator(const Path &path) : mImpl(std::make_unique<Impl>())
{
    mImpl->Iter = std::filesystem::recursive_directory_iterator(path.mImpl->Path);
}

RecursiveDirectoryIterator::~RecursiveDirectoryIterator()
{
}

RecursiveDirectoryIterator::RecursiveDirectoryIterator(RecursiveDirectoryIterator &&) noexcept            = default;
RecursiveDirectoryIterator &RecursiveDirectoryIterator::operator=(RecursiveDirectoryIterator &&) noexcept = default;

Path RecursiveDirectoryIterator::operator*() const
{
    Path ret{};
    ret.mImpl->Path = mImpl->Iter->path();

    return ret;
}

RecursiveDirectoryIterator &RecursiveDirectoryIterator::operator++()
{
    if (!mImpl)
        return *this;

    std::error_code errorCode;
    mImpl->Iter.increment(errorCode);

    bool pastEnd = mImpl->Iter == std::filesystem::recursive_directory_iterator{};

    if (errorCode || pastEnd)
    {
        // Reset internal state. We do this because we use
        // nullptr pimpl as end of  iteration:
        mImpl.reset();
    }

    return *this;
}

bool RecursiveDirectoryIterator::operator!=(const RecursiveDirectoryIterator &other) const
{
    return mImpl != other.mImpl;
}