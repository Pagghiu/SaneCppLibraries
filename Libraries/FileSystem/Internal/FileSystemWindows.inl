// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// clang-format off
#include <stdio.h>
#include <wchar.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
// clang-format on

#include "../../File/FileDescriptor.h"
#include "../../Foundation/Deferred.h"
#include "../FileSystem.h"

namespace SC
{
struct UtilityWindows
{
    [[nodiscard]] static Result formatWindowsError(int errorNumber, String& buffer)
    {
        LPWSTR messageBuffer = nullptr;
        size_t size          = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
            errorNumber, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&messageBuffer), 0, NULL);
        auto deferFree = MakeDeferred([&]() { LocalFree(messageBuffer); });

        const StringView sv = StringView(Span<const wchar_t>(messageBuffer, size), true);
        if (not buffer.assign(sv))
        {
            return Result::Error("UtilityWindows::formatWindowsError - returned error");
        }
        return Result(true);
    }
};

static constexpr const SC::Result getErrorCode(int errorCode)
{
    switch (errorCode)
    {
    case EEXIST: return Result::Error("EEXIST");
    case ENOENT: return Result::Error("ENOENT");
    }
    return Result::Error("Unknown");
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

    [[nodiscard]] static bool existsAndIsLink(const wchar_t* dir)
    {
        const DWORD res = GetFileAttributesW(dir);
        if (res == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }
        return (res & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    }

    [[nodiscard]] static bool moveDirectory(const wchar_t* sourcePath, const wchar_t* destinationPath)
    {
        // Use MOVEFILE_COPY_ALLOWED for moving across volumes
        return ::MoveFileEx(sourcePath, destinationPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) == TRUE;
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
        SC_TRY(buffer.data.resizeWithoutInitializing(buffer.data.capacity()));
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

#if SC_FILESYSTEM_WINDOWS_USE_SHELL_OPERATIONS
    [[nodiscard]] static Result copyDirectory(String& sourceDirectory, String& destinationDirectory,
                                              FileSystem::CopyFlags options)
    {
        SHFILEOPSTRUCTW shFileOp;
        memset(&shFileOp, 0, sizeof(shFileOp));
        const wchar_t* dest = destinationDirectory.view().getNullTerminatedNative();
        if (not options.overwrite)
        {
            if (existsAndIsDirectory(dest))
            {
                return Result::Error("Directory already exists");
            }
            else if (existsAndIsFile(dest))
            {
                return Result::Error("A file already exists at the location");
            }
        }
        shFileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NO_UI;
        shFileOp.wFunc  = FO_COPY;
        SC_TRY(StringConverter(sourceDirectory).appendNullTerminated(L"\\*\0"));
        SC_TRY(StringConverter(destinationDirectory).appendNullTerminated(L"\0"));
        // SHFileOperationW needs two null termination bytes
        shFileOp.pFrom = sourceDirectory.view().getNullTerminatedNative();
        shFileOp.pTo   = dest;
        const int res  = SHFileOperationW(&shFileOp);
        return Result(res == 0);
    }

    [[nodiscard]] static Result removeDirectoryRecursive(String& sourceDirectory)
    {
        SHFILEOPSTRUCTW shFileOp;
        memset(&shFileOp, 0, sizeof(shFileOp));
        shFileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NO_UI;
        shFileOp.wFunc  = FO_DELETE;
        // SHFileOperationW needs two null termination bytes
        SC_TRY(StringConverter(sourceDirectory).appendNullTerminated(L"\0"));
        shFileOp.pFrom = sourceDirectory.view().getNullTerminatedNative();
        const int res  = SHFileOperationW(&shFileOp);
        return Result(res == 0);
    }
#endif

    [[nodiscard]] static Result getFileTime(const wchar_t* file, FileTime& time)
    {
        HANDLE hFile =
            CreateFileW(file, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        FileDescriptor deferFileClose = hFile;
        SC_TRY_MSG(deferFileClose.isValid(), "getFileTime: Invalid file");
        FILETIME creationTime, lastAccessTime, modifiedTime;
        if (GetFileTime(hFile, &creationTime, &lastAccessTime, &modifiedTime))
        {
            ULARGE_INTEGER fileTimeValue;
            fileTimeValue.LowPart  = modifiedTime.dwLowDateTime;
            fileTimeValue.HighPart = modifiedTime.dwHighDateTime;
            fileTimeValue.QuadPart -= 116444736000000000ULL;
            time.modifiedTime = Time::Absolute(fileTimeValue.QuadPart / 10000ULL);
            return Result(true);
        }
        return Result(false);
    }

    [[nodiscard]] static Result setLastModifiedTime(const wchar_t* file, Time::Absolute time)
    {
        HANDLE hFile = CreateFileW(file, FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, NULL);

        FileDescriptor deferFileClose = hFile;
        SC_TRY_MSG(deferFileClose.isValid(), "setLastModifiedTime: Invalid file");

        FILETIME creationTime, lastAccessTime;
        if (!GetFileTime(hFile, &creationTime, &lastAccessTime, NULL))
        {
            return Result(false);
        }

        FILETIME       modifiedTime;
        ULARGE_INTEGER fileTimeValue;
        fileTimeValue.QuadPart      = time.getMillisecondsSinceEpoch() * 10000ULL + 116444736000000000ULL;
        modifiedTime.dwLowDateTime  = fileTimeValue.LowPart;
        modifiedTime.dwHighDateTime = fileTimeValue.HighPart;
        if (!SetFileTime(hFile, &creationTime, &lastAccessTime, &modifiedTime))
        {
            return Result(false);
        }

        return Result(true);
    }
#undef SC_TRY_LIBC
};
