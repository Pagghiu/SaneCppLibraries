// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/FixedSizePimpl.h"
#include "../Foundation/Result.h"
#include "../Foundation/StringView.h"
#include "FileDescriptor.h"

namespace SC
{
struct FileSystemWalker;
} // namespace SC

struct SC::FileSystemWalker
{
    enum class Type
    {
        Directory,
        File
    };
    struct Entry
    {
        StringView     name;
        StringView     path;
        uint32_t       level = 0;
        FileDescriptor parentFileDescriptor;
        Type           type = Type::File;
        bool           isDirectory() const { return type == Type::Directory; }
    };
    struct Options
    {
        bool recursive = false;
    };

    Options options;

    FileSystemWalker();
    ~FileSystemWalker();
    FileSystemWalker(const FileSystemWalker&)            = delete;
    FileSystemWalker& operator=(const FileSystemWalker&) = delete;
    FileSystemWalker(FileSystemWalker&&)                 = delete;
    FileSystemWalker& operator=(FileSystemWalker&&)      = delete;

    const Entry& get() const { return entry; }

    [[nodiscard]] ReturnCode checkErrors()
    {
        errorsChecked = true;
        return errorResult;
    }
    [[nodiscard]] ReturnCode init(StringView directory);

    /// Returned string is only valid until next enumerateNext call and/or another init call
    [[nodiscard]] ReturnCode enumerateNext();

    [[nodiscard]] ReturnCode recurseSubdirectory();

  private:
    struct Internal;
#if SC_PLATFORM_WINDOWS
    FixedSizePimpl<Internal, 256 * sizeof(void*) + (512 + 128) * sizeof(wchar_t)> internal;
#else
    FixedSizePimpl<Internal, 256 * sizeof(void*) + (512 + 128) * sizeof(char)> internal;
#endif

    Entry      entry;
    ReturnCode errorResult;
    bool       errorsChecked = false;
};
