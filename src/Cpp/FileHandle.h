#pragma once

#include <cstdint>
#include <memory>
#include <string>

// Pimpl based wrapper around fstream.
// Does nothing clever with the functions, just hermetizes
// to reduce compilation times. I make the assumption
// that one heap allocation is not a heavy cost
// for file opening, so I am using traditional pimple idiom.
// Using in-place pimpl in here is even uglier than usual
// since sizeof(std::ifstream) varies across compilers.
//
// TODO: Currently assumes binary mode usage.
class FileHandle {
  public:
    enum class Dir
    {
        Beg,
        Cur,
        End
    };

  public:
    // Input path is assumed to be utf8 encoded:
    FileHandle(const std::string &path);
    // Explicitly defining destruct for pimpl idiom:
    // (otherwise due to inline linkage there is
    // incomplete type error in orhter translation units).
    ~FileHandle();

    FileHandle(const FileHandle &)            = delete;
    FileHandle &operator=(const FileHandle &) = delete;
    // Same story as destructor:
    FileHandle(FileHandle &&) noexcept;
    FileHandle &operator=(FileHandle &&) noexcept;

    bool    Good();
    int64_t Size();
    int64_t TellG();
    void    SeekG(int64_t offset, Dir dir);
    void    Read(char *memory, int64_t size);
    int64_t GCount();
    void    Clear();

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};