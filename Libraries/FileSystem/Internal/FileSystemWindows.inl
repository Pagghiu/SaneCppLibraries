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

#include "../../File/FileDescriptor.h"
#include "../FileSystem.h"

#ifndef SC_FILESYSTEM_WINDOWS_USE_SHELL_OPERATIONS
#define SC_FILESYSTEM_WINDOWS_USE_SHELL_OPERATIONS 1
#elif SC_FILESYSTEM_WINDOWS_USE_SHELL_OPERATIONS
#include "../../Containers/SmallVector.h"
#include "../FileSystemWalker.h"
#endif

namespace SC
{

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

    [[nodiscard]] static Result copyDirectory(String& sourceDirectory, String& destinationDirectory,
                                              FileSystem::CopyFlags options)
    {
#if SC_FILESYSTEM_WINDOWS_USE_SHELL_OPERATIONS
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
#else
        FileSystemWalker fsWalker;
        StringView       sourceView = sourceDirectory.view();
        SC_TRY(fsWalker.init(sourceView));
        fsWalker.options.recursive        = true;
        StringNative<512> destinationPath = StringEncoding::Native;
        if (not existsAndIsDirectory(destinationDirectory.view().getNullTerminatedNative()))
        {
            SC_TRY(makeDirectory(destinationDirectory.view().getNullTerminatedNative()));
        }
        while (fsWalker.enumerateNext())
        {
            const FileSystemWalker::Entry& entry       = fsWalker.get();
            StringView                     partialPath = entry.path.sliceStartBytes(sourceView.sizeInBytes());
            StringConverter                destinationConvert(destinationPath, StringConverter::Clear);
            SC_TRY(destinationConvert.appendNullTerminated(destinationDirectory.view()));
            SC_TRY(destinationConvert.appendNullTerminated(partialPath));
            if (entry.isDirectory())
            {
                if (options.overwrite)
                {
                    // We don't care about the result
                    if (existsAndIsFile(destinationPath.view().getNullTerminatedNative()))
                    {
                        (void)removeFile(destinationPath.view().getNullTerminatedNative());
                    }
                }
                (void)makeDirectory(destinationPath.view().getNullTerminatedNative());
                SC_TRY(existsAndIsDirectory(destinationPath.view().getNullTerminatedNative()));
            }
            else
            {
                SC_TRY(copyFile(entry.path, destinationPath.view(), options));
            }
        }
        SC_TRY(fsWalker.checkErrors());
        return Result(true);
#endif
    }

    [[nodiscard]] static Result removeDirectoryRecursive(String& sourceDirectory)
    {
#if SC_FILESYSTEM_WINDOWS_USE_SHELL_OPERATIONS
        SHFILEOPSTRUCTW shFileOp;
        memset(&shFileOp, 0, sizeof(shFileOp));
        shFileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NO_UI;
        shFileOp.wFunc  = FO_DELETE;
        // SHFileOperationW needs two null termination bytes
        SC_TRY(StringConverter(sourceDirectory).appendNullTerminated(L"\0"));
        shFileOp.pFrom = sourceDirectory.view().getNullTerminatedNative();
        const int res  = SHFileOperationW(&shFileOp);
        return Result(res == 0);
#else
        FileSystemWalker fsWalker;
        SC_TRY(fsWalker.init(sourceDirectory.view()));
        fsWalker.options.recursive = true;
        SmallVector<StringNative<512>, 64> emptyDirectories;
        while (fsWalker.enumerateNext())
        {
            const FileSystemWalker::Entry& entry = fsWalker.get();
            if (entry.isDirectory())
            {
                SC_TRY(emptyDirectories.push_back(entry.path));
            }
            else
            {
                SC_TRY(removeFile(entry.path.getNullTerminatedNative()));
            }
        }
        SC_TRY(fsWalker.checkErrors());

        while (not emptyDirectories.isEmpty())
        {
            SC_TRY(removeEmptyDirectory(emptyDirectories.back().view().getNullTerminatedNative()));
            SC_TRY(emptyDirectories.pop_back());
        }
        SC_TRY(removeEmptyDirectory(sourceDirectory.view().getNullTerminatedNative()));
        return Result(true);
#endif
    }

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
            time.modifiedTime = AbsoluteTime(fileTimeValue.QuadPart / 10000ULL);
            return Result(true);
        }
        return Result(false);
    }

    [[nodiscard]] static Result setLastModifiedTime(const wchar_t* file, AbsoluteTime time)
    {
        HANDLE         hFile          = CreateFileW(file, FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
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
