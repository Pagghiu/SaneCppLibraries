// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
// clang-format off
#include <stdio.h>
#include <wchar.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
// clang-format on

#include "FileSystem.h"

namespace SC
{

static constexpr const SC::StringView getErrorCode(int errorCode)
{
    switch (errorCode)
    {
    case EEXIST: return "EEXIST"_a8;
    case ENOENT: return "ENOENT"_a8;
    }
    return "Unknown"_a8;
}
} // namespace SC

struct SC::FileSystem::Internal
{
#define SC_TRY_LIBC(func)                                                                                              \
    {                                                                                                                  \
        if (func != 0)                                                                                                 \
        {                                                                                                              \
            return false;                                                                                              \
        }                                                                                                              \
    }

    [[nodiscard]] static bool makeDirectory(const wchar_t* dir)
    {
        SC_TRY_LIBC(_wmkdir(dir));
        return true;
    }

    [[nodiscard]] static bool exists(const wchar_t* fileOrDirectory)
    {
        const DWORD res = GetFileAttributesW(fileOrDirectory);
        return res != INVALID_FILE_ATTRIBUTES;
    }

    [[nodiscard]] static bool existsAndIsDirectory(const wchar_t* dir)
    {
        const DWORD res = GetFileAttributesW(dir);
        if (res == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }
        return (res & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    [[nodiscard]] static bool existsAndIsFile(const wchar_t* dir)
    {
        const DWORD res = GetFileAttributesW(dir);
        if (res == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }
        return (res & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    [[nodiscard]] static bool removeEmptyDirectory(const wchar_t* dir)
    {
        SC_TRY_LIBC(_wrmdir(dir));
        return true;
    }

    [[nodiscard]] static bool removeFile(const wchar_t* file)
    {
        SC_TRY_LIBC(_wremove(file));
        return true;
    }

    [[nodiscard]] static bool openFileRead(const wchar_t* path, FILE*& file)
    {
        return _wfopen_s(&file, path, L"rb") == 0;
    }

    [[nodiscard]] static bool openFileWrite(const wchar_t* path, FILE*& file)
    {
        return _wfopen_s(&file, path, L"wb") == 0;
    }

    [[nodiscard]] static bool formatError(int errorNumber, String& buffer)
    {
        buffer.encoding = StringEncoding::Utf16;
        SC_TRY_IF(buffer.data.resizeWithoutInitializing(buffer.data.capacity()));
        const int res = _wcserror_s(buffer.nativeWritableBytesIncludingTerminator(),
                                    buffer.sizeInBytesIncludingTerminator() / sizeof(wchar_t), errorNumber);
        if (res == 0)
        {
            const size_t numUtf16Points = wcslen(buffer.nativeWritableBytesIncludingTerminator()) + 1;
            return buffer.data.resizeWithoutInitializing(numUtf16Points * sizeof(wchar_t));
        }
        SC_TRUST_RESULT(buffer.data.resizeWithoutInitializing(0));
        return false;
    }

    [[nodiscard]] static bool copyFile(const StringView& source, const StringView& destination,
                                       FileSystem::CopyFlags options)
    {
        DWORD flags = COPY_FILE_FAIL_IF_EXISTS;
        if (options.overwrite)
            flags &= ~COPY_FILE_FAIL_IF_EXISTS;
        BOOL res = CopyFileExW(source.getNullTerminatedNative(), destination.getNullTerminatedNative(), nullptr,
                               nullptr, nullptr, flags);
        return res == TRUE;
    }

    // TODO: replace SHFileOperationW with something else as it's slow at first use( it loads a lot of DLLs) and ASAN
    // complains
    [[nodiscard]] static ReturnCode copyDirectory(String& sourceDirectory, String& destinationDirectory,
                                                  FileSystem::CopyFlags options)
    {
        SHFILEOPSTRUCTW shFileOp;
        memset(&shFileOp, 0, sizeof(shFileOp));
        const wchar_t* dest = destinationDirectory.view().getNullTerminatedNative();
        if (not options.overwrite)
        {
            if (existsAndIsDirectory(dest))
            {
                return "Directory already exists"_a8;
            }
            else if (existsAndIsFile(dest))
            {
                return "A file already exists at the location"_a8;
            }
        }
        shFileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NO_UI;
        shFileOp.wFunc  = FO_COPY;
        SC_TRY_IF(StringConverter(sourceDirectory).appendNullTerminated(L"\\*"));
        // SHFileOperationW needs two null termination bytes
        SC_TRY_IF(sourceDirectory.pushNullTerm());
        SC_TRY_IF(destinationDirectory.pushNullTerm());
        shFileOp.pFrom = sourceDirectory.view().getNullTerminatedNative();
        shFileOp.pTo   = dest;
        const int res  = SHFileOperationW(&shFileOp);
        return res == 0;
    }

    [[nodiscard]] static ReturnCode removeDirectoryRecursive(String& sourceDirectory)
    {
        SHFILEOPSTRUCTW shFileOp;
        memset(&shFileOp, 0, sizeof(shFileOp));
        shFileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NO_UI;
        shFileOp.wFunc  = FO_DELETE;
        // SHFileOperationW needs two null termination bytes
        SC_TRY_IF(sourceDirectory.pushNullTerm());
        shFileOp.pFrom = sourceDirectory.view().getNullTerminatedNative();
        const int res  = SHFileOperationW(&shFileOp);
        return res == 0;
    }
#undef SC_TRY_LIBC
};
