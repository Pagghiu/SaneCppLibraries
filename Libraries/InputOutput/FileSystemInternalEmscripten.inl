// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystem.h"
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h> // mkdir

static constexpr SC::ReturnCode getErrorCode(int errorCode) { return "Unknown"_a8; }

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

    [[nodiscard]] static bool copyFile(const char* sourceFile, const char* destinationFile,
                                       FileSystem::CopyFlags options)
    {
        return false;
    }

    template <int N>
    [[nodiscard]] static bool copyDirectory(StringNative<N>& sourceFile, StringNative<N>& destinationFile,
                                            FileSystem::CopyFlags options)
    {
        return false;
    }

    template <int N>
    static bool removeDirectoryRecursive(StringNative<N>& directory)
    {
        return false;
    }
};
