// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "FileSystem.h"
#include "../File/FileDescriptor.h"
#include "../FileSystem/Path.h"
#include "../Strings/StringConverter.h"
#include "FileSystemOperations.h"

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
    [[nodiscard]] static Result formatWindowsError(int errorNumber, String& buffer)
    {
        LPWSTR messageBuffer = nullptr;
        size_t size          = ::FormatMessageW(
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

    [[nodiscard]] static bool formatError(int errorNumber, String& buffer)
    {
        buffer.encoding = StringEncoding::Utf8;
        SC_TRY(buffer.data.resizeWithoutInitializing(buffer.data.capacity()));
#if SC_PLATFORM_APPLE
        const int res = ::strerror_r(errorNumber, buffer.nativeWritableBytesIncludingTerminator(),
                                     buffer.sizeInBytesIncludingTerminator());
        if (res == 0)
        {
            return buffer.data.resizeWithoutInitializing(strlen(buffer.nativeWritableBytesIncludingTerminator()) + 1);
        }
        SC_TRUST_RESULT(buffer.data.resizeWithoutInitializing(0));
        return false;
#else
        char* res = strerror_r(errorNumber, buffer.nativeWritableBytesIncludingTerminator(),
                               buffer.sizeInBytesIncludingTerminator());

        if (res != buffer.nativeWritableBytesIncludingTerminator())
        {
            return buffer.assign(StringView({res, strlen(res)}, true, StringEncoding::Utf8));
        }
        return true;

#endif
    }
};
#endif
SC::Result SC::FileSystem::init(StringView currentWorkingDirectory) { return changeDirectory(currentWorkingDirectory); }

SC::Result SC::FileSystem::changeDirectory(StringView currentWorkingDirectory)
{
    StringConverter converter(currentDirectory, StringConverter::Clear);
    SC_TRY(converter.appendNullTerminated(currentWorkingDirectory));
    // TODO: Assert if path is not absolute
    return Result(existsAndIsDirectory("."));
}

bool SC::FileSystem::convert(const StringView file, String& destination, StringView* encodedPath)
{
    if (Path::isAbsolute(file, Path::AsNative))
    {
        if (encodedPath != nullptr)
        {
            StringConverter converter(destination);
            return converter.convertNullTerminateFastPath(file, *encodedPath);
        }
        StringConverter converter(destination, StringConverter::Clear);
        return converter.appendNullTerminated(file);
    }
    if (currentDirectory.isEmpty())
        return false;
    StringConverter converter(destination, StringConverter::Clear);
    SC_TRY(converter.appendNullTerminated(currentDirectory.view()));
#if SC_PLATFORM_WINDOWS
    SC_TRY(converter.appendNullTerminated(L"\\"));
#else
    SC_TRY(converter.appendNullTerminated("/"));
#endif
    SC_TRY(converter.appendNullTerminated(file));
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

SC::Result SC::FileSystem::write(StringView path, Span<const char> data)
{
    StringView encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
    FileDescriptor fd;
    SC_TRY(fd.open(encodedPath, FileOpen::Write));
    return fd.write(data);
}

SC::Result SC::FileSystem::write(StringView path, Span<const uint8_t> data)
{
    return write(path, {reinterpret_cast<const char*>(data.data()), data.sizeInBytes()});
}

SC::Result SC::FileSystem::read(StringView path, Buffer& data)
{
    StringView encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
    FileDescriptor fd;
    SC_TRY(fd.open(encodedPath, FileOpen::Read));
    return fd.readUntilEOF(data);
}

[[nodiscard]] SC::Result SC::FileSystem::writeString(StringView path, StringView text)
{
    return write(path, text.toCharSpan());
}

[[nodiscard]] SC::Result SC::FileSystem::writeStringAppend(StringView path, StringView text)
{
    StringView encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
    FileDescriptor fd;
    SC_TRY(fd.open(encodedPath, FileOpen::Append));
    return fd.write(text.toCharSpan());
}

[[nodiscard]] SC::Result SC::FileSystem::read(StringView path, String& text, StringEncoding encoding)
{
    text.data.clear();
    text.encoding = encoding;
    StringView encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
    FileDescriptor fd;
    SC_TRY(fd.open(encodedPath, FileOpen::Read));
    return fd.readUntilEOF(text);
}

SC::Result SC::FileSystem::formatError(int errorNumber, StringView item, bool isWindowsNativeError)
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
    StringConverter errorMessage(errorMessageBuffer);
    SC_TRY(errorMessage.appendNullTerminated(" for \""));
    SC_TRY(errorMessage.appendNullTerminated(item));
    SC_TRY(errorMessage.appendNullTerminated("\""));
    return Result::FromStableCharPointer(errorMessageBuffer.view().bytesIncludingTerminator());
}

SC::Result SC::FileSystem::rename(StringView path, StringView newPath)
{
    StringView encodedPath1, encodedPath2;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath1));
    SC_TRY(convert(newPath, fileFormatBuffer2, &encodedPath2));
    return FileSystemOperations::rename(encodedPath1, encodedPath2);
}

SC::Result SC::FileSystem::removeFiles(Span<const StringView> files)
{
    StringView encodedPath;
    for (auto& path : files)
    {
        SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, FileSystemOperations::removeFile(encodedPath));
    }
    return Result(true);
}

SC::Result SC::FileSystem::removeFileIfExists(StringView source)
{
    if (existsAndIsFile(source))
        return removeFiles(Span<const StringView>{source});
    return Result(true);
}

SC::Result SC::FileSystem::removeLinkIfExists(StringView source)
{
    if (existsAndIsLink(source))
        return removeFiles(Span<const StringView>{source});
    return Result(true);
}

SC::Result SC::FileSystem::removeDirectoriesRecursive(Span<const StringView> directories)
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
    if (currentDirectory.isEmpty())
        return Result(false);
    StringView encodedPath1, encodedPath2;
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
    if (currentDirectory.isEmpty())
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

SC::Result SC::FileSystem::removeEmptyDirectories(Span<const StringView> directories)
{
    StringView encodedPath;
    for (StringView path : directories)
    {
        SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, FileSystemOperations::removeEmptyDirectory(encodedPath));
    }
    return Result(true);
}

SC::Result SC::FileSystem::removeEmptyDirectoriesRecursive(Span<const StringView> directories)
{
    StringView encodedPath;
    for (StringView path : directories)
    {
        SC_TRY(convert(path, fileFormatBuffer2, &encodedPath));
        int dirnameLevels = 0;
        while (FileSystemOperations::removeEmptyDirectory(encodedPath))
        {
            encodedPath = Path::dirname(encodedPath, Path::AsNative, dirnameLevels);
            dirnameLevels += 1;
            SC_TRY(convert(encodedPath, fileFormatBuffer1, &encodedPath));
        }
    }
    return Result(true);
}

SC::Result SC::FileSystem::makeDirectories(Span<const StringView> directories)
{
    StringView encodedPath;
    for (auto& path : directories)
    {
        SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, FileSystemOperations::makeDirectory(encodedPath));
    }
    return Result(true);
}

SC::Result SC::FileSystem::makeDirectoriesRecursive(Span<const StringView> directories)
{
    for (const auto& path : directories)
    {
        StringView encodedPath;
        SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY(FileSystemOperations::makeDirectoryRecursive(encodedPath));
    }
    return Result(true);
}

SC::Result SC::FileSystem::makeDirectoriesIfNotExists(Span<const StringView> directories)
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

SC::Result SC::FileSystem::createSymbolicLink(StringView sourceFileOrDirectory, StringView linkFile)
{
    StringView sourceFileNative, linkFileNative;
    SC_TRY(convert(sourceFileOrDirectory, fileFormatBuffer1, &sourceFileNative));
    SC_TRY(convert(linkFile, fileFormatBuffer2, &linkFileNative));
    SC_TRY(FileSystemOperations::createSymbolicLink(sourceFileNative, linkFileNative));
    return Result(true);
}

[[nodiscard]] bool SC::FileSystem::exists(StringView fileOrDirectory)
{
    StringView encodedPath;
    SC_TRY(convert(fileOrDirectory, fileFormatBuffer1, &encodedPath));
    return FileSystemOperations::exists(encodedPath);
}

bool SC::FileSystem::existsAndIsDirectory(StringView directory)
{
    StringView encodedPath;
    SC_TRY(convert(directory, fileFormatBuffer1, &encodedPath));
    return FileSystemOperations::existsAndIsDirectory(encodedPath);
}

[[nodiscard]] bool SC::FileSystem::existsAndIsFile(StringView file)
{
    StringView encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    return FileSystemOperations::existsAndIsFile(encodedPath);
}

[[nodiscard]] bool SC::FileSystem::existsAndIsLink(StringView file)
{
    StringView encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    return FileSystemOperations::existsAndIsLink(encodedPath);
}

[[nodiscard]] bool SC::FileSystem::moveDirectory(StringView sourceDirectory, StringView destinationDirectory)
{
    StringView encodedPath1;
    StringView encodedPath2;
    SC_TRY(convert(sourceDirectory, fileFormatBuffer1, &encodedPath1));
    SC_TRY(convert(destinationDirectory, fileFormatBuffer2, &encodedPath2));
    return FileSystemOperations::moveDirectory(encodedPath1, encodedPath2);
}

SC::Result SC::FileSystem::getFileStat(StringView file, FileStat& fileStat)
{
    StringView encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    SC_TRY(FileSystemOperations::getFileStat(encodedPath, fileStat));
    return Result(true);
}

SC::Result SC::FileSystem::setLastModifiedTime(StringView file, Time::Realtime time)
{
    StringView encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    return FileSystemOperations::setLastModifiedTime(encodedPath, time);
}

#undef SC_TRY_FORMAT_ERRNO
