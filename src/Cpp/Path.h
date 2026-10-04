#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <fstream>

std::u8string_view Utf8FromString(const std::string& str);

std::string_view CharFromUtf8String(const std::u8string &u8str);

// Pimpl-based wrapper around std::filesystem::path
// meant to imporve compilation times.
// Currently I don't plan to do file operations
// in a hot loop, so additional allocation
// and indirection penalty still seems
// like a good tradeoff.
// Everything is returned in terms of std::strings
// which internally are utf8 encoded.
class Path {
  public:
    static Path Current();

    // This constructor allocates a default-valued
    // impl struct and initializes pimpl with it:
    Path();
    // Input string is assumed to be utf8 encoded:
    Path(const std::string &pathStr);
    // Explicitly defining destructor for pimpl idiom:
    // (otherwise due to inline linkage there is
    // incomplete type error in other translation units).
    ~Path();

    Path(const Path &);
    Path &operator=(const Path &);
    // Default moves, same story as destructor:
    Path(Path &&) noexcept;
    Path &operator=(Path &&) noexcept;

    [[nodiscard]] Path Parent() const;
    // Returns this path, reltive to base one:
    [[nodiscard]] Path Relative(const Path& base) const;

    [[nodiscard]] bool Exists() const;
    [[nodiscard]] bool IsRegularFile() const;
    [[nodiscard]] bool IsDirectory() const;

    // Returns last write time as number of
    // microseconds since epoch start:
    [[nodiscard]] int64_t LastWriteTime() const;

    void CreateDirectory() const;

    // Returns entire path converted to utf8 encoded string:
    [[nodiscard]] std::u8string U8String() const;
    // Returns entire path converted to utf8 encoded string:
    [[nodiscard]] std::string String() const;
    // Returns filename as utf8-encoded string:
    [[nodiscard]] std::string Filename() const;
    // Returns stem as utf8-encoded string:
    [[nodiscard]] std::string Stem() const;
    // Returns extension as utf8-encoded string:
    [[nodiscard]] std::string Extension() const;

    [[nodiscard]] std::ifstream Open() const;

    // Wrappers around operator overloads for paths:
    friend Path operator/(const Path &lhs, const Path&rhs);
    friend bool operator==(const Path &lhs, const Path &rhs);

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
    friend class DirectoryIterator;
    friend class RecursiveDirectoryIterator;
};

class DirectoryIterator {
  public:
    // This constructor leaves pimpl as nullptr,
    // which we exploit as end-of-iteration value:
    DirectoryIterator();
    DirectoryIterator(const Path &path);
    // Default destructor, same as above:
    ~DirectoryIterator();

    DirectoryIterator(const DirectoryIterator &)            = delete;
    DirectoryIterator &operator=(const DirectoryIterator &) = delete;
    DirectoryIterator(DirectoryIterator &&) noexcept;
    DirectoryIterator &operator=(DirectoryIterator &&) noexcept;

    DirectoryIterator &operator++();
    Path               operator*() const;
    bool               operator!=(const DirectoryIterator &other) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

class DirectoryRange {
  public:
    explicit DirectoryRange(const Path &path) : mPath(path)
    {
    }
    [[nodiscard]] DirectoryIterator begin() const
    {
        return {mPath};
    }
    [[nodiscard]] DirectoryIterator end() const
    {
        return {};
    }

  private:
    const Path &mPath;
};

class RecursiveDirectoryIterator {
  public:
    // This constructor leaves pimpl as nullptr,
    // which we exploit as end-of-iteration value:
    RecursiveDirectoryIterator();
    RecursiveDirectoryIterator(const Path &path);
    // Default destructor, same as above:
    ~RecursiveDirectoryIterator();

    RecursiveDirectoryIterator(const RecursiveDirectoryIterator &)            = delete;
    RecursiveDirectoryIterator &operator=(const RecursiveDirectoryIterator &) = delete;
    RecursiveDirectoryIterator(RecursiveDirectoryIterator &&) noexcept;
    RecursiveDirectoryIterator &operator=(RecursiveDirectoryIterator &&) noexcept;

    RecursiveDirectoryIterator &operator++();
    Path               operator*() const;
    bool               operator!=(const RecursiveDirectoryIterator &other) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

class RecursiveDirectoryRange {
  public:
    explicit RecursiveDirectoryRange(const Path &path) : mPath(path)
    {
    }
    [[nodiscard]] RecursiveDirectoryIterator begin() const
    {
        return {mPath};
    }
    [[nodiscard]] RecursiveDirectoryIterator end() const
    {
        return {};
    }

  private:
    const Path &mPath;
};