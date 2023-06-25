// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystem.h"
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h> // mkdir

namespace SC
{
static constexpr SC::ReturnCode getErrorCode(int errorCode) { return "Unknown"_a8; }
} // namespace SC

struct SC::FileSystem::Internal
{
    [[nodiscard]] static bool makeDirectory(const char* dir) { return false; }

    [[nodiscard]] static bool exists(const char* path) { return false; }

    [[nodiscard]] static bool existsAndIsDirectory(const char* path) { return false; }

    [[nodiscard]] static bool existsAndIsFile(const char* path) { return false; }

    [[nodiscard]] static bool removeEmptyDirectory(const char* path) { return false; }

    [[nodiscard]] static bool removeFile(const char* path) { return false; }

    [[nodiscard]] static bool openFileRead(const char* path, FILE*& file) { return false; }

    [[nodiscard]] static bool openFileWrite(const char* path, FILE*& file) { return false; }

    [[nodiscard]] static bool formatError(int errorNumber, String& buffer) { return false; }

    [[nodiscard]] static bool copyFile(const StringView& sourceFile, const StringView& destinationFile,
                                       FileSystem::CopyFlags options)
    {
        return false;
    }

    [[nodiscard]] static bool copyDirectory(String& sourceFile, String& destinationFile, FileSystem::CopyFlags options)
    {
        return false;
    }

    static bool removeDirectoryRecursive(String& directory) { return false; }

    [[nodiscard]] static ReturnCode getFileTime(const char* file, FileTime& time) { return false; }

    [[nodiscard]] static ReturnCode setLastModifiedTime(const char* file, AbsoluteTime time) { return false; }
};
