// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystem/FileSystem.h"
#include "../File/FileDescriptor.h"
#include "../FileSystem/FileSystemOperations.h"

#if _WIN32
#include "../Foundation/Deferred.h"
#include <stdio.h>
#include <wchar.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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
    static Result formatWindowsError(int errorNumber, Span<char> buffer)
    {
        LPWSTR messageBuffer = nullptr;
        size_t size          = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
            errorNumber, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&messageBuffer), 0, NULL);
        auto deferFree = MakeDeferred([&]() { LocalFree(messageBuffer); });

        // TODO: Write buffer from messageBuffer converting from UTF-16 to UTF-8
        if (size == 0)
        {
            return Result::Error("SC::FileSystem::Internal::formatWindowsError - Cannot format error");
        }
        int res = ::MultiByteToWideChar(CP_UTF8, 0, buffer.data(), static_cast<int>(buffer.sizeInBytes()),
                                        messageBuffer, static_cast<int>(size));
        return Result(res > 0);
    }
    static bool formatError(int errorNumber, Span<char> buffer)
    {
        wchar_t   messageBuffer[1024];
        const int err = ::_wcserror_s(messageBuffer, sizeof(messageBuffer) / sizeof(wchar_t), errorNumber);
        if (err == 0)
        {
            const int messageLength = static_cast<int>(::wcsnlen_s(messageBuffer, 1024));
            const int res = ::MultiByteToWideChar(CP_UTF8, 0, buffer.data(), static_cast<int>(buffer.sizeInBytes()),
                                                  messageBuffer, messageLength);
            return Result(res > 0);
        }
        return false;
    }
};

#else
#include <errno.h>  // errno
#include <string.h> // strerror_r
namespace SC
{
// This is shared with FileSystemIterator
Result getErrorCode(int errorCode)
{
    switch (errorCode)
    {
    case EACCES: return Result::Error("EACCES");
    case EDQUOT: return Result::Error("EDQUOT");
    case EEXIST: return Result::Error("EEXIST");
    case EFAULT: return Result::Error("EFAULT");
    case EIO: return Result::Error("EIO");
    case ELOOP: return Result::Error("ELOOP");
    case EMLINK: return Result::Error("EMLINK");
    case ENAMETOOLONG: return Result::Error("ENAMETOOLONG");
    case ENOENT: return Result::Error("ENOENT");
    case ENOSPC: return Result::Error("ENOSPC");
    case ENOTDIR: return Result::Error("ENOTDIR");
    case EROFS: return Result::Error("EROFS");
    case EBADF: return Result::Error("EBADF");
    case EPERM: return Result::Error("EPERM");
    case ENOMEM: return Result::Error("ENOMEM");
    case ENOTSUP: return Result::Error("ENOTSUP");
    case EINVAL: return Result::Error("EINVAL");
    }
    return Result::Error("Unknown");
}
} // namespace SC

struct SC::FileSystem::Internal
{

    static bool formatError(int errorNumber, Span<native_char_t> buffer)
    {
#if SC_PLATFORM_APPLE
        const int res = ::strerror_r(errorNumber, buffer.data(), buffer.sizeInBytes());
        return res == 0;
#else
        char* res = ::strerror_r(errorNumber, buffer.data(), buffer.sizeInBytes());
        return res != buffer.data();
#endif
    }
};
#endif
SC::Result SC::FileSystem::init(StringSpan currentWorkingDirectory) { return changeDirectory(currentWorkingDirectory); }

SC::Result SC::FileSystem::changeDirectory(StringSpan currentWorkingDirectory)
{
    SC_TRY_MSG(currentDirectory.assign(currentWorkingDirectory),
               "FileSystem::changeDirectory - Cannot assign working directory");
    // TODO: Assert if path is not absolute
    return Result(existsAndIsDirectory("."));
}

bool SC::FileSystem::convert(const StringSpan file, StringPath& destination, StringSpan* encodedPath)
{
    SC_TRY(destination.assign(file));
    if (encodedPath)
    {
        *encodedPath = destination.view();
    }

#if SC_PLATFORM_WINDOWS
    const bool absolute = (destination.length >= 2 and destination.path[0] == L'\\' and destination.path[1] == L'\\') or
                          (destination.length >= 2 and destination.path[1] == L':');
#else
    const bool absolute = destination.length >= 1 and destination.path[0] == '/';
#endif
    if (absolute)
    {
        if (encodedPath != nullptr)
        {
            *encodedPath = destination;
        }
        return true;
    }
    if (currentDirectory.length == 0)
        return false;

    StringPath relative = destination;
    destination         = currentDirectory;
#if SC_PLATFORM_WINDOWS
    destination.path[destination.length] = L'\\';
    ::memcpy(destination.path + destination.length + 1, relative.path, relative.length * sizeof(wchar_t));
#else
    destination.path[destination.length] = '/';
    ::memcpy(destination.path + destination.length + 1, relative.path, relative.length);
#endif
    destination.path[destination.length + relative.length + 1] = 0;
    destination.length += relative.length + 1;
    if (encodedPath != nullptr)
    {
        *encodedPath = destination.view();
    }
    return true;
}

#define SC_TRY_FORMAT_ERRNO(path, func)                                                                                \
    {                                                                                                                  \
        if (not func)                                                                                                  \
        {                                                                                                              \
            return formatError(errno, path, false);                                                                    \
        }                                                                                                              \
    }
#if SC_PLATFORM_WINDOWS
#define SC_TRY_FORMAT_NATIVE(path, func)                                                                               \
    {                                                                                                                  \
        auto tempRes = func;                                                                                           \
        if (not tempRes)                                                                                               \
        {                                                                                                              \
            if (TypeTraits::IsSame<decltype(tempRes), Result>::value)                                                  \
            {                                                                                                          \
                return Result(tempRes);                                                                                \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                return formatError(GetLastError(), path, true);                                                        \
            }                                                                                                          \
        }                                                                                                              \
    }
#else
#define SC_TRY_FORMAT_NATIVE(path, func)                                                                               \
    {                                                                                                                  \
        auto tempRes = func;                                                                                           \
        if (not tempRes)                                                                                               \
        {                                                                                                              \
            if (SC::TypeTraits::IsSame<decltype(tempRes), Result>::value)                                              \
            {                                                                                                          \
                return Result(tempRes);                                                                                \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                return formatError(errno, path, false);                                                                \
            }                                                                                                          \
        }                                                                                                              \
    }
#endif

SC::Result SC::FileSystem::write(StringSpan path, Span<const char> data)
{
    StringSpan encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
    FileDescriptor fd;
    SC_TRY(fd.open(encodedPath, FileOpen::Write));
    return fd.write(data);
}

SC::Result SC::FileSystem::write(StringSpan path, Span<const uint8_t> data)
{
    return write(path, {reinterpret_cast<const char*>(data.data()), data.sizeInBytes()});
}

SC::Result SC::FileSystem::writeString(StringSpan path, StringSpan text) { return write(path, text.toCharSpan()); }

SC::Result SC::FileSystem::writeStringAppend(StringSpan path, StringSpan text)
{
    StringSpan encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
    FileDescriptor fd;
    SC_TRY(fd.open(encodedPath, FileOpen::Append));
    return fd.write(text.toCharSpan());
}

SC::Result SC::FileSystem::formatError(int errorNumber, StringSpan item, bool isWindowsNativeError)
{
#if SC_PLATFORM_WINDOWS
    if (isWindowsNativeError)
    {
        if (not preciseErrorMessages)
        {
            return Result::Error("Windows Error");
        }
        if (not Internal::formatWindowsError(errorNumber, errorMessageBuffer))
        {
            return Result::Error("SC::FileSystem::formatError - Cannot format error");
        }
    }
    else
#endif
    {
        SC_COMPILER_UNUSED(isWindowsNativeError);
        if (not preciseErrorMessages)
        {
            return getErrorCode(errorNumber);
        }
        if (not Internal::formatError(errorNumber, errorMessageBuffer))
        {
            return Result::Error("SC::FileSystem::formatError - Cannot format error");
        }
    }
    (void)item; // TODO: Append item name
    return Result::FromStableCharPointer(errorMessageBuffer);
}

SC::Result SC::FileSystem::rename(StringSpan path, StringSpan newPath)
{
    StringSpan encodedPath1, encodedPath2;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath1));
    SC_TRY(convert(newPath, fileFormatBuffer2, &encodedPath2));
    return FileSystemOperations::rename(encodedPath1, encodedPath2);
}

SC::Result SC::FileSystem::removeFiles(Span<const StringSpan> files)
{
    StringSpan encodedPath;
    for (auto& path : files)
    {
        SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, FileSystemOperations::removeFile(encodedPath));
    }
    return Result(true);
}

SC::Result SC::FileSystem::removeFileIfExists(StringSpan source)
{
    if (existsAndIsFile(source))
        return removeFiles(Span<const StringSpan>{source});
    return Result(true);
}

SC::Result SC::FileSystem::removeLinkIfExists(StringSpan source)
{
    if (existsAndIsLink(source))
        return removeFiles(Span<const StringSpan>{source});
    return Result(true);
}

SC::Result SC::FileSystem::removeDirectoriesRecursive(Span<const StringSpan> directories)
{
    for (auto& path : directories)
    {
        SC_TRY(convert(path, fileFormatBuffer1)); // force write
        SC_TRY_FORMAT_ERRNO(path, FileSystemOperations::removeDirectoryRecursive(fileFormatBuffer1.view()));
    }
    return Result(true);
}

SC::Result SC::FileSystem::copyFiles(Span<const CopyOperation> sourceDestination)
{
    if (currentDirectory.length == 0)
        return Result(false);
    StringSpan encodedPath1, encodedPath2;
    for (const CopyOperation& op : sourceDestination)
    {
        SC_TRY(convert(op.source, fileFormatBuffer1, &encodedPath1));
        SC_TRY(convert(op.destination, fileFormatBuffer2, &encodedPath2));
        SC_TRY_FORMAT_NATIVE(op.source, FileSystemOperations::copyFile(encodedPath1, encodedPath2, op.copyFlags));
    }
    return Result(true);
}

SC::Result SC::FileSystem::copyDirectories(Span<const CopyOperation> sourceDestination)
{
    if (currentDirectory.length == 0)
        return Result(false);
    for (const CopyOperation& op : sourceDestination)
    {
        SC_TRY(convert(op.source, fileFormatBuffer1));      // force write
        SC_TRY(convert(op.destination, fileFormatBuffer2)); // force write
        SC_TRY_FORMAT_NATIVE(op.source, FileSystemOperations::copyDirectory(fileFormatBuffer1.view(),
                                                                            fileFormatBuffer2.view(), op.copyFlags));
    }
    return Result(true);
}

SC::Result SC::FileSystem::removeEmptyDirectories(Span<const StringSpan> directories)
{
    StringSpan encodedPath;
    for (StringSpan path : directories)
    {
        SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, FileSystemOperations::removeEmptyDirectory(encodedPath));
    }
    return Result(true);
}

SC::Result SC::FileSystem::makeDirectories(Span<const StringSpan> directories)
{
    StringSpan encodedPath;
    for (auto& path : directories)
    {
        SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, FileSystemOperations::makeDirectory(encodedPath));
    }
    return Result(true);
}

SC::Result SC::FileSystem::makeDirectoriesRecursive(Span<const StringSpan> directories)
{
    for (const auto& path : directories)
    {
        StringSpan encodedPath;
        SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY(FileSystemOperations::makeDirectoryRecursive(encodedPath));
    }
    return Result(true);
}

SC::Result SC::FileSystem::makeDirectoriesIfNotExists(Span<const StringSpan> directories)
{
    for (const auto& path : directories)
    {
        if (not existsAndIsDirectory(path))
        {
            SC_TRY(makeDirectory({path}));
        }
    }
    return Result(true);
}

SC::Result SC::FileSystem::createSymbolicLink(StringSpan sourceFileOrDirectory, StringSpan linkFile)
{
    StringSpan sourceFileNative, linkFileNative;
    SC_TRY(convert(sourceFileOrDirectory, fileFormatBuffer1, &sourceFileNative));
    SC_TRY(convert(linkFile, fileFormatBuffer2, &linkFileNative));
    SC_TRY(FileSystemOperations::createSymbolicLink(sourceFileNative, linkFileNative));
    return Result(true);
}

bool SC::FileSystem::exists(StringSpan fileOrDirectory)
{
    StringSpan encodedPath;
    SC_TRY(convert(fileOrDirectory, fileFormatBuffer1, &encodedPath));
    return FileSystemOperations::exists(encodedPath);
}

bool SC::FileSystem::existsAndIsDirectory(StringSpan directory)
{
    StringSpan encodedPath;
    SC_TRY(convert(directory, fileFormatBuffer1, &encodedPath));
    return FileSystemOperations::existsAndIsDirectory(encodedPath);
}

bool SC::FileSystem::existsAndIsFile(StringSpan file)
{
    StringSpan encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    return FileSystemOperations::existsAndIsFile(encodedPath);
}

bool SC::FileSystem::existsAndIsLink(StringSpan file)
{
    StringSpan encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    return FileSystemOperations::existsAndIsLink(encodedPath);
}

bool SC::FileSystem::moveDirectory(StringSpan sourceDirectory, StringSpan destinationDirectory)
{
    StringSpan encodedPath1;
    StringSpan encodedPath2;
    SC_TRY(convert(sourceDirectory, fileFormatBuffer1, &encodedPath1));
    SC_TRY(convert(destinationDirectory, fileFormatBuffer2, &encodedPath2));
    return FileSystemOperations::moveDirectory(encodedPath1, encodedPath2);
}

SC::Result SC::FileSystem::getFileStat(StringSpan file, FileStat& fileStat)
{
    StringSpan encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    SC_TRY(FileSystemOperations::getFileStat(encodedPath, fileStat));
    return Result(true);
}

SC::Result SC::FileSystem::setLastModifiedTime(StringSpan file, Time::Realtime time)
{
    StringSpan encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    return FileSystemOperations::setLastModifiedTime(encodedPath, time);
}

#undef SC_TRY_FORMAT_ERRNO
