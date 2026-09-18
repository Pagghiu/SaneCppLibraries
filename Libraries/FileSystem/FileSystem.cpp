// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystem/FileSystem.h"
#include "../Common/Deferred.h"

#if _WIN32
#include "../Common/Deferred.h"
#include <errno.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winioctl.h>

namespace SC
{
namespace FileSystemWindowsDetail
{
#include "../Common/WindowsPath.inl"
}
} // namespace SC

namespace
{
static SC::ResultFileSystem fileSystemResultFromNative(SC::uint32_t nativeError, SC::FileSystemErrorDetail detail)
{
    using SC::FileSystemError;
    switch (nativeError)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return SC::ResultFileSystem::withNativeError(FileSystemError::EntryNotFound, detail, nativeError);
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        return SC::ResultFileSystem::withNativeError(FileSystemError::EntryAlreadyExists, detail, nativeError);
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return SC::ResultFileSystem::withNativeError(FileSystemError::AccessDenied, detail, nativeError);
    case ERROR_WRITE_PROTECT:
        return SC::ResultFileSystem::withNativeError(FileSystemError::ReadOnlyFileSystem, detail, nativeError);
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return SC::ResultFileSystem::withNativeError(FileSystemError::StorageCapacityExceeded, detail, nativeError);
    case ERROR_DIR_NOT_EMPTY:
        return SC::ResultFileSystem::withNativeError(FileSystemError::DirectoryNotEmpty, detail, nativeError);
    case ERROR_TOO_MANY_LINKS:
        return SC::ResultFileSystem::withNativeError(FileSystemError::TooManyLinks, detail, nativeError);
    case ERROR_CANT_RESOLVE_FILENAME:
        return SC::ResultFileSystem::withNativeError(FileSystemError::SymbolicLinkLoop, detail, nativeError);
    case ERROR_NOT_SAME_DEVICE:
        return SC::ResultFileSystem::withNativeError(FileSystemError::CrossDeviceOperation, detail, nativeError);
    case ERROR_INVALID_PARAMETER:
        return SC::ResultFileSystem::withNativeError(FileSystemError::InvalidArgument, detail, nativeError);
    case ERROR_NOT_SUPPORTED:
    case ERROR_CALL_NOT_IMPLEMENTED:
        return SC::ResultFileSystem::withNativeError(FileSystemError::OperationUnsupported, detail, nativeError);
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return SC::ResultFileSystem::withNativeError(FileSystemError::OutOfMemory, detail, nativeError);
    case ERROR_FILENAME_EXCED_RANGE:
        return SC::ResultFileSystem::withNativeError(FileSystemError::PathCapacityExceeded, detail, nativeError);
    case ERROR_FILE_TOO_LARGE:
        return SC::ResultFileSystem::withNativeError(FileSystemError::FileTooLarge, detail, nativeError);
    case ERROR_DIRECTORY:
        return SC::ResultFileSystem::withNativeError(FileSystemError::EntryTypeMismatch, detail, nativeError);
    case ERROR_IO_DEVICE:
    case ERROR_CRC:
    case ERROR_READ_FAULT:
    case ERROR_WRITE_FAULT:
    case ERROR_SEEK: return SC::ResultFileSystem::withNativeError(FileSystemError::IoFailure, detail, nativeError);
    }
    return SC::ResultFileSystem::withNativeError(FileSystemError::OperationFailed, detail, nativeError);
}

static SC::ResultFileSystem translateWindowsPathError(SC::FileSystemWindowsDetail::WindowsPathResult result,
                                                      SC::FileSystemErrorDetail                      detail)
{
    using SC::FileSystemError;
    using SC::FileSystemWindowsDetail::WindowsPathError;
    switch (result.error)
    {
    case WindowsPathError::CapacityExceeded: return {FileSystemError::PathCapacityExceeded, detail};
    case WindowsPathError::BasePathNotAbsolute: return {FileSystemError::PathMustBeAbsolute, detail};
    case WindowsPathError::MalformedPath: return {FileSystemError::InvalidPath, detail};
    case WindowsPathError::NativeCallFailed:
        return SC::ResultFileSystem::withNativeError(FileSystemError::OperationFailed, detail, result.nativeError);
    case WindowsPathError::None: return {};
    }
    return {FileSystemError::InvalidPath, detail};
}
} // namespace

#else
#include <errno.h>    // errno
#include <fcntl.h>    // open, O_*
#include <string.h>   // strerror_r
#include <sys/stat.h> // stat, fstat
#include <unistd.h>   // write, close, read

namespace
{
static SC::ResultFileSystem fileSystemResultFromNative(SC::uint32_t nativeError, SC::FileSystemErrorDetail detail)
{
    using SC::FileSystemError;
    switch (static_cast<int>(nativeError))
    {
    case EACCES:
    case EPERM: return SC::ResultFileSystem::withNativeError(FileSystemError::AccessDenied, detail, nativeError);
    case EDQUOT:
    case ENOSPC:
        return SC::ResultFileSystem::withNativeError(FileSystemError::StorageCapacityExceeded, detail, nativeError);
    case EEXIST: return SC::ResultFileSystem::withNativeError(FileSystemError::EntryAlreadyExists, detail, nativeError);
    case EFAULT:
    case EINVAL: return SC::ResultFileSystem::withNativeError(FileSystemError::InvalidArgument, detail, nativeError);
    case EIO: return SC::ResultFileSystem::withNativeError(FileSystemError::IoFailure, detail, nativeError);
    case ELOOP: return SC::ResultFileSystem::withNativeError(FileSystemError::SymbolicLinkLoop, detail, nativeError);
    case EMLINK: return SC::ResultFileSystem::withNativeError(FileSystemError::TooManyLinks, detail, nativeError);
    case ENAMETOOLONG:
        return SC::ResultFileSystem::withNativeError(FileSystemError::PathCapacityExceeded, detail, nativeError);
    case ENOENT: return SC::ResultFileSystem::withNativeError(FileSystemError::EntryNotFound, detail, nativeError);
    case ENOTDIR:
    case EISDIR: return SC::ResultFileSystem::withNativeError(FileSystemError::EntryTypeMismatch, detail, nativeError);
    case EROFS: return SC::ResultFileSystem::withNativeError(FileSystemError::ReadOnlyFileSystem, detail, nativeError);
    case EBADF: return SC::ResultFileSystem::withNativeError(FileSystemError::InvalidState, detail, nativeError);
    case ENOMEM: return SC::ResultFileSystem::withNativeError(FileSystemError::OutOfMemory, detail, nativeError);
    case ENOTSUP:
        return SC::ResultFileSystem::withNativeError(FileSystemError::OperationUnsupported, detail, nativeError);
    case EFBIG: return SC::ResultFileSystem::withNativeError(FileSystemError::FileTooLarge, detail, nativeError);
    case ENOTEMPTY:
        return SC::ResultFileSystem::withNativeError(FileSystemError::DirectoryNotEmpty, detail, nativeError);
    case EXDEV:
        return SC::ResultFileSystem::withNativeError(FileSystemError::CrossDeviceOperation, detail, nativeError);
    }
    return SC::ResultFileSystem::withNativeError(FileSystemError::OperationFailed, detail, nativeError);
}
} // namespace
#endif
SC::ResultFileSystem SC::FileSystem::init(StringSpan currentWorkingDirectory)
{
    return changeDirectory(currentWorkingDirectory);
}

SC::ResultFileSystem SC::FileSystem::changeDirectory(StringSpan currentWorkingDirectory)
{
#if SC_PLATFORM_WINDOWS
    const auto pathResult = FileSystemWindowsDetail::WindowsPath::makeAbsoluteLogicalPath(
        currentWorkingDirectory, currentDirectory.view(), currentDirectory);
    if (not pathResult)
        return translateWindowsPathError(pathResult, FileSystemErrorDetail::NormalizePath);
#else
    if (not currentDirectory.assign(currentWorkingDirectory))
        return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::ChangeDirectory};
#endif
    if (not existsAndIsDirectory("."))
        return {FileSystemError::EntryNotFound, FileSystemErrorDetail::ChangeDirectory};
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::convert(const StringSpan file, StringPath& destination,
                                             StringNativeBuffer<StringPath::MaxPath + 6 + 1>& transportPath,
                                             StringSpan*                                      encodedPath)
{
#if SC_PLATFORM_WINDOWS
    if (currentDirectory.view().isEmpty())
    {
        const auto logicalPathResult = FileSystemWindowsDetail::WindowsPath::makeLogicalPath(file, destination);
        if (not logicalPathResult)
            return translateWindowsPathError(logicalPathResult, FileSystemErrorDetail::NormalizePath);
        if (not FileSystemWindowsDetail::WindowsPath::isAbsolute(destination.view()))
            return {FileSystemError::NotInitialized, FileSystemErrorDetail::BuildTransportPath};
    }
    const auto pathResult = FileSystemWindowsDetail::WindowsPath::makeTransportPath(file, currentDirectory.view(),
                                                                                    destination, transportPath);
    if (not pathResult)
        return translateWindowsPathError(pathResult, FileSystemErrorDetail::BuildTransportPath);
    if (encodedPath != nullptr)
    {
        *encodedPath = transportPath.view();
    }
    return Result(true);
#else
    (void)(transportPath);
    if (not destination.assign(file))
        return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::BuildTransportPath};
    if (encodedPath)
    {
        *encodedPath = destination.view();
    }

    auto       destinationBuffer = destination.writableSpan().data();
    const bool absolute          = not destination.view().isEmpty() and destinationBuffer[0] == '/';
    if (absolute)
    {
        if (encodedPath != nullptr)
        {
            *encodedPath = destination.view();
        }
        return Result(true);
    }
    if (currentDirectory.view().isEmpty())
        return {FileSystemError::NotInitialized, FileSystemErrorDetail::BuildTransportPath};

    StringPath   relative = destination;
    const size_t requiredBytes =
        currentDirectory.view().sizeInBytes() + sizeof(native_char_t) + relative.view().sizeInBytes();
    if (requiredBytes / sizeof(native_char_t) > StringPath::MaxPath)
    {
        if (requiredBytes <= 0xffffffffu)
            return ResultFileSystem::withRequiredBytes(FileSystemError::PathCapacityExceeded,
                                                       FileSystemErrorDetail::BuildTransportPath,
                                                       static_cast<uint32_t>(requiredBytes));
        return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::BuildTransportPath};
    }
    if (not destination.assign(currentDirectory.view()))
        return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::BuildTransportPath};
    destinationBuffer[destination.view().sizeInBytes()] = '/';
    ::memcpy(destinationBuffer + destination.view().sizeInBytes() + 1, relative.view().bytesWithoutTerminator(),
             relative.view().sizeInBytes());
    const size_t lastPos       = destination.view().sizeInBytes() + relative.view().sizeInBytes() + 1;
    destinationBuffer[lastPos] = 0;
    if (not destination.resize(lastPos))
        return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::BuildTransportPath};
    if (encodedPath != nullptr)
    {
        *encodedPath = destination.view();
    }
    return Result(true);
#endif
}

SC::ResultFileSystem SC::FileSystem::write(StringSpan path, Span<const char> data)
{
    StringSpan encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
#if SC_PLATFORM_WINDOWS
    HANDLE hFile = ::CreateFileW(encodedPath.getNullTerminatedNative(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::OpenFileForWrite);
    }
    DWORD bytesWritten;
    if (!::WriteFile(hFile, data.data(), static_cast<DWORD>(data.sizeInBytes()), &bytesWritten, nullptr))
    {
        const DWORD nativeError = ::GetLastError();
        ::CloseHandle(hFile);
        return fileSystemResultFromNative(nativeError, FileSystemErrorDetail::WriteFileContent);
    }
    ::CloseHandle(hFile);
    if (bytesWritten != data.sizeInBytes())
    {
        return ResultFileSystem::withActualBytes(FileSystemError::IncompleteWrite,
                                                 FileSystemErrorDetail::WriteFileContent, bytesWritten);
    }
    return Result(true);
#else
    int fd = ::open(encodedPath.getNullTerminatedNative(), O_WRONLY | O_CREAT | O_TRUNC,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::OpenFileForWrite);
    }
    ssize_t   bytesWritten = ::write(fd, data.data(), data.sizeInBytes());
    const int nativeError  = errno;
    ::close(fd);
    if (bytesWritten == -1)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(nativeError), FileSystemErrorDetail::WriteFileContent);
    }
    if (static_cast<size_t>(bytesWritten) != data.sizeInBytes())
    {
        if (static_cast<uint64_t>(bytesWritten) <= 0xffffffffu)
            return ResultFileSystem::withActualBytes(FileSystemError::IncompleteWrite,
                                                     FileSystemErrorDetail::WriteFileContent,
                                                     static_cast<uint32_t>(bytesWritten));
        return {FileSystemError::IncompleteWrite, FileSystemErrorDetail::WriteFileContent};
    }
    return Result(true);
#endif
}

SC::ResultFileSystem SC::FileSystem::write(StringSpan path, Span<const uint8_t> data)
{
    return write(path, {reinterpret_cast<const char*>(data.data()), data.sizeInBytes()});
}

SC::ResultFileSystem SC::FileSystem::writeString(StringSpan path, StringSpan text)
{
    return write(path, text.toCharSpan());
}

SC::ResultFileSystem SC::FileSystem::writeStringAppend(StringSpan path, StringSpan text)
{
    StringSpan encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
#if SC_PLATFORM_WINDOWS
    HANDLE hFile = ::CreateFileW(encodedPath.getNullTerminatedNative(), FILE_APPEND_DATA, 0, nullptr, OPEN_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::OpenFileForWrite);
    }
    DWORD bytesWritten;
    if (!::WriteFile(hFile, text.bytesWithoutTerminator(), static_cast<DWORD>(text.sizeInBytes()), &bytesWritten,
                     nullptr))
    {
        const DWORD nativeError = ::GetLastError();
        ::CloseHandle(hFile);
        return fileSystemResultFromNative(nativeError, FileSystemErrorDetail::AppendFileContent);
    }
    ::CloseHandle(hFile);
    if (bytesWritten != text.sizeInBytes())
    {
        return ResultFileSystem::withActualBytes(FileSystemError::IncompleteWrite,
                                                 FileSystemErrorDetail::AppendFileContent, bytesWritten);
    }
    return Result(true);
#else
    int fd = ::open(encodedPath.getNullTerminatedNative(), O_WRONLY | O_CREAT | O_APPEND,
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::OpenFileForWrite);
    }
    ssize_t   bytesWritten = ::write(fd, text.bytesWithoutTerminator(), text.sizeInBytes());
    const int nativeError  = errno;
    ::close(fd);
    if (bytesWritten == -1)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(nativeError), FileSystemErrorDetail::AppendFileContent);
    }
    if (static_cast<size_t>(bytesWritten) != text.sizeInBytes())
    {
        if (static_cast<uint64_t>(bytesWritten) <= 0xffffffffu)
            return ResultFileSystem::withActualBytes(FileSystemError::IncompleteWrite,
                                                     FileSystemErrorDetail::AppendFileContent,
                                                     static_cast<uint32_t>(bytesWritten));
        return {FileSystemError::IncompleteWrite, FileSystemErrorDetail::AppendFileContent};
    }
    return Result(true);
#endif
}

SC::ResultFileSystem SC::FileSystem::read(StringSpan path, IGrowableBuffer&& buffer)
{
    StringSpan encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
#if SC_PLATFORM_WINDOWS
    HANDLE hFile = ::CreateFileW(encodedPath.getNullTerminatedNative(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::OpenFileForRead);
    }
    auto deferClose = MakeDeferred([&]() { ::CloseHandle(hFile); });

    // Get file size
    LARGE_INTEGER fileSize;
    if (!::GetFileSizeEx(hFile, &fileSize))
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::QueryFileSize);
    }

    // Grow buffer to accommodate the file
    if (!buffer.resizeWithoutInitializing(static_cast<size_t>(fileSize.QuadPart)))
    {
        if (fileSize.QuadPart >= 0 and static_cast<uint64_t>(fileSize.QuadPart) <= 0xffffffffu)
            return ResultFileSystem::withRequiredBytes(FileSystemError::BufferCapacityExceeded,
                                                       FileSystemErrorDetail::GrowReadBuffer,
                                                       static_cast<uint32_t>(fileSize.QuadPart));
        return {FileSystemError::BufferCapacityExceeded, FileSystemErrorDetail::GrowReadBuffer};
    }

    // Read the file
    DWORD bytesRead;
    if (!::ReadFile(hFile, buffer.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr))
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::ReadFileContent);
    }

    if (bytesRead != static_cast<DWORD>(fileSize.QuadPart))
    {
        return ResultFileSystem::withActualBytes(FileSystemError::IncompleteRead,
                                                 FileSystemErrorDetail::ReadFileContent, bytesRead);
    }

    return Result(true);
#else
    int fd = ::open(encodedPath.getNullTerminatedNative(), O_RDONLY);
    if (fd == -1)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::OpenFileForRead);
    }
    auto deferClose = MakeDeferred([&]() { ::close(fd); });

    // Get file size
    struct stat fileStat;
    if (::fstat(fd, &fileStat) == -1)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::QueryFileSize);
    }

    // Grow buffer to accommodate the file
    if (!buffer.resizeWithoutInitializing(static_cast<size_t>(fileStat.st_size)))
    {
        if (fileStat.st_size >= 0 and static_cast<uint64_t>(fileStat.st_size) <= 0xffffffffu)
            return ResultFileSystem::withRequiredBytes(FileSystemError::BufferCapacityExceeded,
                                                       FileSystemErrorDetail::GrowReadBuffer,
                                                       static_cast<uint32_t>(fileStat.st_size));
        return {FileSystemError::BufferCapacityExceeded, FileSystemErrorDetail::GrowReadBuffer};
    }

    // Read the file
    ssize_t bytesRead = ::read(fd, buffer.data(), static_cast<size_t>(fileStat.st_size));
    if (bytesRead == -1)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::ReadFileContent);
    }

    if (static_cast<size_t>(bytesRead) != static_cast<size_t>(fileStat.st_size))
    {
        if (static_cast<uint64_t>(bytesRead) <= 0xffffffffu)
            return ResultFileSystem::withActualBytes(FileSystemError::IncompleteRead,
                                                     FileSystemErrorDetail::ReadFileContent,
                                                     static_cast<uint32_t>(bytesRead));
        return {FileSystemError::IncompleteRead, FileSystemErrorDetail::ReadFileContent};
    }

    return Result(true);
#endif
}

SC::ResultFileSystem SC::FileSystem::rename(StringSpan path, StringSpan newPath)
{
    StringSpan encodedPath1, encodedPath2;
    SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath1));
    SC_TRY(convert(newPath, fileFormatBuffer2, fileTransportBuffer2, &encodedPath2));
    return FileSystem::Operations::rename(encodedPath1, encodedPath2);
}

SC::ResultFileSystem SC::FileSystem::removeFiles(Span<const StringSpan> files)
{
    StringSpan encodedPath;
    for (auto& path : files)
    {
        SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
        SC_TRY(FileSystem::Operations::removeFile(encodedPath));
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::removeFileIfExists(StringSpan source)
{
    if (existsAndIsFile(source))
        return removeFiles(Span<const StringSpan>{source});
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::removeLinkIfExists(StringSpan source)
{
    if (existsAndIsLink(source))
    {
#if SC_PLATFORM_WINDOWS
        if (existsAndIsDirectory(source))
        {
            return removeEmptyDirectories(Span<const StringSpan>{source});
        }
#endif
        return removeFiles(Span<const StringSpan>{source});
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::removeDirectoriesRecursive(Span<const StringSpan> directories)
{
    for (auto& path : directories)
    {
        StringSpan encodedPath;
        SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
        SC_TRY(FileSystem::Operations::removeDirectoryRecursive(encodedPath));
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::copyFiles(Span<const CopyOperation> sourceDestination)
{
    if (currentDirectory.view().isEmpty())
        return Result(false);
    StringSpan encodedPath1, encodedPath2;
    for (const CopyOperation& op : sourceDestination)
    {
        SC_TRY(convert(op.source, fileFormatBuffer1, fileTransportBuffer1, &encodedPath1));
        SC_TRY(convert(op.destination, fileFormatBuffer2, fileTransportBuffer2, &encodedPath2));
        SC_TRY(FileSystem::Operations::copyFile(encodedPath1, encodedPath2, op.copyFlags));
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::copyDirectories(Span<const CopyOperation> sourceDestination)
{
    if (currentDirectory.view().isEmpty())
        return Result(false);
    for (const CopyOperation& op : sourceDestination)
    {
        StringSpan encodedPath1;
        StringSpan encodedPath2;
        SC_TRY(convert(op.source, fileFormatBuffer1, fileTransportBuffer1, &encodedPath1));
        SC_TRY(convert(op.destination, fileFormatBuffer2, fileTransportBuffer2, &encodedPath2));
        SC_TRY(FileSystem::Operations::copyDirectory(encodedPath1, encodedPath2, op.copyFlags));
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::removeEmptyDirectories(Span<const StringSpan> directories)
{
    StringSpan encodedPath;
    for (StringSpan path : directories)
    {
        SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
        SC_TRY(FileSystem::Operations::removeEmptyDirectory(encodedPath));
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::makeDirectories(Span<const StringSpan> directories)
{
    StringSpan encodedPath;
    for (auto& path : directories)
    {
        SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
        SC_TRY(FileSystem::Operations::makeDirectory(encodedPath));
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::makeDirectoriesRecursive(Span<const StringSpan> directories)
{
    for (const auto& path : directories)
    {
        StringSpan encodedPath;
        SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
        SC_TRY(FileSystem::Operations::makeDirectoryRecursive(encodedPath));
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::makeDirectoriesIfNotExists(Span<const StringSpan> directories)
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

SC::ResultFileSystem SC::FileSystem::createSymbolicLink(StringSpan sourceFileOrDirectory, StringSpan linkFile)
{
    StringSpan sourceFileNative, linkFileNative;
    SC_TRY(convert(sourceFileOrDirectory, fileFormatBuffer1, fileTransportBuffer1, &sourceFileNative));
    SC_TRY(convert(linkFile, fileFormatBuffer2, fileTransportBuffer2, &linkFileNative));
    SC_TRY(FileSystem::Operations::createSymbolicLink(sourceFileNative, linkFileNative));
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::createHardLink(StringSpan sourceFile, StringSpan linkFile)
{
    StringSpan sourceFileNative, linkFileNative;
    SC_TRY(convert(sourceFile, fileFormatBuffer1, fileTransportBuffer1, &sourceFileNative));
    SC_TRY(convert(linkFile, fileFormatBuffer2, fileTransportBuffer2, &linkFileNative));
    SC_TRY(FileSystem::Operations::createHardLink(sourceFileNative, linkFileNative));
    return Result(true);
}

bool SC::FileSystem::exists(StringSpan fileOrDirectory)
{
    StringSpan encodedPath;
    if (not convert(fileOrDirectory, fileFormatBuffer1, fileTransportBuffer1, &encodedPath))
        return false;
    return FileSystem::Operations::exists(encodedPath);
}

bool SC::FileSystem::existsAndIsDirectory(StringSpan directory)
{
    StringSpan encodedPath;
    if (not convert(directory, fileFormatBuffer1, fileTransportBuffer1, &encodedPath))
        return false;
    return FileSystem::Operations::existsAndIsDirectory(encodedPath);
}

bool SC::FileSystem::existsAndIsFile(StringSpan file)
{
    StringSpan encodedPath;
    if (not convert(file, fileFormatBuffer1, fileTransportBuffer1, &encodedPath))
        return false;
    return FileSystem::Operations::existsAndIsFile(encodedPath);
}

bool SC::FileSystem::existsAndIsLink(StringSpan file)
{
    StringSpan encodedPath;
    if (not convert(file, fileFormatBuffer1, fileTransportBuffer1, &encodedPath))
        return false;
    return FileSystem::Operations::existsAndIsLink(encodedPath);
}

bool SC::FileSystem::canAccess(StringSpan fileOrDirectory, AccessMode accessMode)
{
    StringSpan encodedPath;
    if (not convert(fileOrDirectory, fileFormatBuffer1, fileTransportBuffer1, &encodedPath))
        return false;
    return FileSystem::Operations::access(encodedPath, accessMode);
}

bool SC::FileSystem::moveDirectory(StringSpan sourceDirectory, StringSpan destinationDirectory)
{
    StringSpan encodedPath1;
    StringSpan encodedPath2;
    if (not convert(sourceDirectory, fileFormatBuffer1, fileTransportBuffer1, &encodedPath1))
        return false;
    if (not convert(destinationDirectory, fileFormatBuffer2, fileTransportBuffer2, &encodedPath2))
        return false;
    return static_cast<bool>(FileSystem::Operations::moveDirectory(encodedPath1, encodedPath2));
}

SC::ResultFileSystem SC::FileSystem::getFileStat(StringSpan file, FileStat& fileStat) { return stat(file, fileStat); }

SC::ResultFileSystem SC::FileSystem::stat(StringSpan file, FileStat& fileStat)
{
    StringSpan encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    SC_TRY(FileSystem::Operations::stat(encodedPath, fileStat));
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::lstat(StringSpan file, FileStat& fileStat)
{
    StringSpan encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    SC_TRY(FileSystem::Operations::lstat(encodedPath, fileStat));
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::readSymbolicLink(StringSpan linkFile, StringPath& destination)
{
    StringSpan encodedPath;
    SC_TRY(convert(linkFile, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    SC_TRY(FileSystem::Operations::readSymbolicLink(encodedPath, destination));
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::chmod(StringSpan path, uint32_t mode)
{
    StringSpan encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    return FileSystem::Operations::chmod(encodedPath, mode);
}

SC::ResultFileSystem SC::FileSystem::chown(StringSpan path, uint32_t uid, uint32_t gid)
{
    StringSpan encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    return FileSystem::Operations::chown(encodedPath, uid, gid);
}

SC::ResultFileSystem SC::FileSystem::lchown(StringSpan path, uint32_t uid, uint32_t gid)
{
    StringSpan encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    return FileSystem::Operations::lchown(encodedPath, uid, gid);
}

SC::ResultFileSystem SC::FileSystem::lchmod(StringSpan path, uint32_t mode)
{
    StringSpan encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    return FileSystem::Operations::lchmod(encodedPath, mode);
}

SC::ResultFileSystem SC::FileSystem::setLastModifiedTime(StringSpan file, TimeMs time)
{
    StringSpan encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    return FileSystem::Operations::setLastModifiedTime(encodedPath, time);
}

#ifdef _WIN32

struct SC::FileSystem::Operations::Internal
{
    static ResultFileSystem validatePath(StringSpan path)
    {
        if (path.sizeInBytes() == 0)
            return Result::Error("Path is empty");
        if (path.getEncoding() != StringEncoding::Utf16)
            return Result::Error("Path is not native (UTF16)");
        return Result(true);
    }

    static size_t skipDirectoryRoot(const wchar_t* path, size_t pathLength)
    {
        if (pathLength >= 8 and path[0] == L'\\' and path[1] == L'\\' and path[2] == L'?' and path[3] == L'\\' and
            (path[4] == L'U' or path[4] == L'u') and (path[5] == L'N' or path[5] == L'n') and
            (path[6] == L'C' or path[6] == L'c') and path[7] == L'\\')
        {
            size_t idx = 8;
            while (idx < pathLength and path[idx] != L'\\' and path[idx] != L'/')
                idx += 1;
            if (idx < pathLength)
                idx += 1;
            while (idx < pathLength and path[idx] != L'\\' and path[idx] != L'/')
                idx += 1;
            if (idx < pathLength)
                idx += 1;
            return idx;
        }
        if (pathLength >= 7 and path[0] == L'\\' and path[1] == L'\\' and path[2] == L'?' and path[3] == L'\\' and
            ((path[4] >= L'a' and path[4] <= L'z') or (path[4] >= L'A' and path[4] <= L'Z')) and path[5] == L':' and
            (path[6] == L'\\' or path[6] == L'/'))
        {
            return 7;
        }
        if (pathLength >= 2 and path[0] == L'\\' and path[1] == L'\\')
        {
            size_t idx = 2;
            while (idx < pathLength and path[idx] != L'\\' and path[idx] != L'/')
                idx += 1;
            if (idx < pathLength)
                idx += 1;
            while (idx < pathLength and path[idx] != L'\\' and path[idx] != L'/')
                idx += 1;
            if (idx < pathLength)
                idx += 1;
            return idx;
        }
        if (pathLength >= 3 and path[1] == L':' and (path[2] == L'\\' or path[2] == L'/'))
        {
            return 3;
        }
        if (pathLength >= 1 and (path[0] == L'\\' or path[0] == L'/'))
        {
            return 1;
        }
        return 0;
    }

    static ResultFileSystem copyFile(StringSpan source, StringSpan destination, FileSystemCopyFlags options,
                                     bool isDirectory = false);

    static ResultFileSystem copyDirectoryRecursive(const wchar_t* source, const wchar_t* destination,
                                                   FileSystemCopyFlags flags);

    static ResultFileSystem removeDirectoryRecursiveInternal(const wchar_t* path);
};

static SC::TimeMs windowsFileTimeToTimeMs(const FILETIME& fileTime)
{
    ULARGE_INTEGER fileTimeValue;
    fileTimeValue.LowPart  = fileTime.dwLowDateTime;
    fileTimeValue.HighPart = fileTime.dwHighDateTime;
    if (fileTimeValue.QuadPart == 0)
    {
        return {};
    }
    fileTimeValue.QuadPart -= 116444736000000000ULL;
    return SC::TimeMs{static_cast<SC::int64_t>(fileTimeValue.QuadPart / 10000ULL)};
}

static SC::FileSystemEntryType windowsEntryTypeFromAttributes(DWORD attributes, DWORD reparseTag)
{
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
    {
        if (reparseTag == IO_REPARSE_TAG_SYMLINK or reparseTag == 0)
        {
            return SC::FileSystemEntryType::SymbolicLink;
        }
        return SC::FileSystemEntryType::Other;
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        return SC::FileSystemEntryType::Directory;
    }
    return SC::FileSystemEntryType::File;
}

static SC::Result fillWindowsFileStat(HANDLE hFile, SC::FileSystemStat& fileStat)
{
    fileStat = {};

    BY_HANDLE_FILE_INFORMATION handleInfo;
    SC_TRY_MSG(::GetFileInformationByHandle(hFile, &handleInfo) != FALSE, "stat: Failed to get file information");
    FILE_STANDARD_INFO standardInfo = {};
    const bool         hasStandardInfo =
        ::GetFileInformationByHandleEx(hFile, FileStandardInfo, &standardInfo, sizeof(standardInfo)) != FALSE;

    FILE_ATTRIBUTE_TAG_INFO tagInfo = {};
    if (::GetFileInformationByHandleEx(hFile, FileAttributeTagInfo, &tagInfo, sizeof(tagInfo)) == FALSE)
    {
        tagInfo.FileAttributes = handleInfo.dwFileAttributes;
        tagInfo.ReparseTag     = 0;
    }

    fileStat.entryType    = windowsEntryTypeFromAttributes(handleInfo.dwFileAttributes, tagInfo.ReparseTag);
    fileStat.creationTime = windowsFileTimeToTimeMs(handleInfo.ftCreationTime);
    fileStat.accessedTime = windowsFileTimeToTimeMs(handleInfo.ftLastAccessTime);
    fileStat.modifiedTime = windowsFileTimeToTimeMs(handleInfo.ftLastWriteTime);
    fileStat.fileSize =
        static_cast<SC::size_t>((static_cast<SC::uint64_t>(handleInfo.nFileSizeHigh) << 32) | handleInfo.nFileSizeLow);
    fileStat.hardLinkCount              = hasStandardInfo ? static_cast<SC::size_t>(standardInfo.NumberOfLinks)
                                                          : static_cast<SC::size_t>(handleInfo.nNumberOfLinks);
    fileStat.windows.attributes         = handleInfo.dwFileAttributes;
    fileStat.windows.reparseTag         = tagInfo.ReparseTag;
    fileStat.windows.volumeSerialNumber = handleInfo.dwVolumeSerialNumber;
    fileStat.windows.fileIndex =
        (static_cast<SC::uint64_t>(handleInfo.nFileIndexHigh) << 32) | handleInfo.nFileIndexLow;
    return SC::Result(true);
}

static SC::Result windowsStat(SC::StringSpan path, bool followLinks, SC::FileSystemStat& fileStat)
{
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (not followLinks)
    {
        flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }

    HANDLE hFile =
        ::CreateFileW(path.getNullTerminatedNative(), FILE_READ_ATTRIBUTES,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, flags, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return SC::Result::Error("stat: Failed to open file");
    }
    auto deferClose = SC::MakeDeferred([&]() { CloseHandle(hFile); });
    return fillWindowsFileStat(hFile, fileStat);
}

static constexpr SC::uint32_t windowsWriteModeBit = 0200u;

struct WindowsReparseDataBuffer
{
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union
    {
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG  Flags;
            WCHAR  PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR  PathBuffer[1];
        } MountPointReparseBuffer;
        struct
        {
            UCHAR DataBuffer[1];
        } GenericReparseBuffer;
    };
};

#define SC_TRY_WIN32(func, msg)                                                                                        \
    {                                                                                                                  \
        if (func == FALSE)                                                                                             \
        {                                                                                                              \
            return Result::Error(msg);                                                                                 \
        }                                                                                                              \
    }

SC::ResultFileSystem SC::FileSystem::Operations::createSymbolicLink(StringSpan sourceFileOrDirectory,
                                                                    StringSpan linkFile)
{
    SC_TRY_MSG(Internal::validatePath(sourceFileOrDirectory), "createSymbolicLink: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(linkFile), "createSymbolicLink: Invalid link path");

    DWORD dwFlags = existsAndIsDirectory(sourceFileOrDirectory) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
    dwFlags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    SC_TRY_WIN32(::CreateSymbolicLinkW(linkFile.getNullTerminatedNative(),
                                       sourceFileOrDirectory.getNullTerminatedNative(), dwFlags),
                 "createSymbolicLink: Failed to create symbolic link");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::createHardLink(StringSpan sourceFile, StringSpan linkFile)
{
    SC_TRY_MSG(Internal::validatePath(sourceFile), "createHardLink: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(linkFile), "createHardLink: Invalid link path");
    SC_TRY_WIN32(::CreateHardLinkW(linkFile.getNullTerminatedNative(), sourceFile.getNullTerminatedNative(), nullptr),
                 "createHardLink: Failed to create hard link");
    return Result(true);
}

SC::Result SC::FileSystem::Operations::access(StringSpan path, AccessMode accessMode)
{
    SC_TRY_MSG(Internal::validatePath(path), "access: Invalid path");

    int mode = 0;
    switch (accessMode)
    {
    case AccessMode::Exists: mode = 0; break;
    case AccessMode::Read: mode = 4; break;
    case AccessMode::Write: mode = 2; break;
    case AccessMode::Execute: mode = 0; break;
    }
    return Result(::_waccess(path.getNullTerminatedNative(), mode) == 0);
}

SC::ResultFileSystem SC::FileSystem::Operations::makeDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "makeDirectory: Invalid path");
    SC_TRY_WIN32(::CreateDirectoryW(path.getNullTerminatedNative(), nullptr),
                 "makeDirectory: Failed to create directory");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::makeDirectoryRecursive(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "makeDirectoryRecursive: Invalid path");
    const size_t pathLength = path.sizeInBytes() / sizeof(wchar_t);
    if (pathLength < 2)
        return Result::Error("makeDirectoryRecursive: Path is empty");
    wchar_t temp[StringPath::MaxPath + 6 + 1] = {};
    // Copy path to temp, ensure null-terminated
    if (pathLength > StringPath::MaxPath + 6)
        return Result::Error("makeDirectoryRecursive: Path too long");
    ::memcpy(temp, path.bytesWithoutTerminator(), pathLength * sizeof(wchar_t));
    temp[pathLength]      = 0; // Ensure null-termination
    const size_t idxStart = Internal::skipDirectoryRoot(temp, pathLength);
    // Iterate and create directories
    for (size_t idx = idxStart; idx < pathLength; ++idx)
    {
        if (temp[idx] == L'\\' or temp[idx] == L'/')
        {
            if (idx == 0)
                continue; // Skip root
            wchar_t old = temp[idx];
            temp[idx]   = 0;
            if (temp[0] != 0) // skip empty
            {
                if (!::CreateDirectoryW(temp, nullptr))
                {
                    DWORD err = ::GetLastError();
                    if (err != ERROR_ALREADY_EXISTS)
                        return Result::Error("makeDirectoryRecursive: Failed to create parent directory");
                }
            }
            temp[idx] = old;
        }
    }
    // Create the final directory
    if (!::CreateDirectoryW(temp, nullptr))
    {
        DWORD err = ::GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
            return Result::Error("makeDirectoryRecursive: Failed to create directory");
    }
    return Result(true);
}

SC::Result SC::FileSystem::Operations::exists(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "exists: Invalid path");
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    return Result(res != INVALID_FILE_ATTRIBUTES);
}

SC::Result SC::FileSystem::Operations::existsAndIsDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsDirectory: Invalid path");
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
        return Result(false);
    return Result((res & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

SC::Result SC::FileSystem::Operations::existsAndIsFile(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsFile: Invalid path");
    const DWORD res = GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
        return Result(false);
    return Result((res & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

SC::Result SC::FileSystem::Operations::existsAndIsLink(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsLink: Invalid path");
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
        return Result(false);
    return Result((res & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
}

SC::ResultFileSystem SC::FileSystem::Operations::readSymbolicLink(StringSpan path, StringPath& destination)
{
    SC_TRY_MSG(Internal::validatePath(path), "readSymbolicLink: Invalid path");

    HANDLE hFile =
        ::CreateFileW(path.getNullTerminatedNative(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return Result::Error("readSymbolicLink: Failed to open link");
    }
    auto deferClose = MakeDeferred([&]() { ::CloseHandle(hFile); });

    alignas(void*) char reparseStorage[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];

    DWORD bytesReturned = 0;
    if (::DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, nullptr, 0, reparseStorage, MAXIMUM_REPARSE_DATA_BUFFER_SIZE,
                          &bytesReturned, nullptr) == FALSE)
    {
        return Result::Error("readSymbolicLink: Failed to query reparse point");
    }

    auto           reparseData = reinterpret_cast<const WindowsReparseDataBuffer*>(reparseStorage);
    const wchar_t* sourcePath  = nullptr;
    size_t         sourceChars = 0;
    if (reparseData->ReparseTag == IO_REPARSE_TAG_SYMLINK)
    {
        const auto& buffer = reparseData->SymbolicLinkReparseBuffer;
        if (buffer.PrintNameLength != 0)
        {
            sourcePath  = buffer.PathBuffer + buffer.PrintNameOffset / sizeof(wchar_t);
            sourceChars = buffer.PrintNameLength / sizeof(wchar_t);
        }
        else
        {
            sourcePath  = buffer.PathBuffer + buffer.SubstituteNameOffset / sizeof(wchar_t);
            sourceChars = buffer.SubstituteNameLength / sizeof(wchar_t);
        }
    }
    else if (reparseData->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
    {
        const auto& buffer = reparseData->MountPointReparseBuffer;
        if (buffer.PrintNameLength != 0)
        {
            sourcePath  = buffer.PathBuffer + buffer.PrintNameOffset / sizeof(wchar_t);
            sourceChars = buffer.PrintNameLength / sizeof(wchar_t);
        }
        else
        {
            sourcePath  = buffer.PathBuffer + buffer.SubstituteNameOffset / sizeof(wchar_t);
            sourceChars = buffer.SubstituteNameLength / sizeof(wchar_t);
        }
    }
    else
    {
        return Result::Error("readSymbolicLink: Unsupported reparse point");
    }

    if (sourceChars > StringPath::MaxPath)
    {
        return Result::Error("readSymbolicLink: Failed to store link target");
    }

    ::memcpy(destination.writableSpan().data(), sourcePath, sourceChars * sizeof(wchar_t));
    destination.writableSpan().data()[sourceChars] = 0;
    SC_TRY_MSG(destination.resize(sourceChars), "readSymbolicLink: Failed to store link target");
    return FileSystemWindowsDetail::WindowsPath::makeLogicalPath(destination.view(), destination);
}

SC::ResultFileSystem SC::FileSystem::Operations::chmod(StringSpan path, uint32_t mode)
{
    SC_TRY_MSG(Internal::validatePath(path), "chmod: Invalid path");
    DWORD attributes = ::GetFileAttributesW(path.getNullTerminatedNative());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return Result::Error("chmod: Failed to read file attributes");
    }
    if (mode & windowsWriteModeBit)
    {
        attributes &= ~FILE_ATTRIBUTE_READONLY;
    }
    else
    {
        attributes |= FILE_ATTRIBUTE_READONLY;
    }
    SC_TRY_WIN32(::SetFileAttributesW(path.getNullTerminatedNative(), attributes), "chmod: Failed to set attributes");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::chown(StringSpan path, uint32_t uid, uint32_t gid)
{
    (void)uid;
    (void)gid;
    SC_TRY_MSG(Internal::validatePath(path), "chown: Invalid path");
    FileSystemStat ignored;
    SC_TRY(windowsStat(path, true, ignored));
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::lchown(StringSpan path, uint32_t uid, uint32_t gid)
{
    (void)uid;
    (void)gid;
    SC_TRY_MSG(Internal::validatePath(path), "lchown: Invalid path");
    FileSystemStat ignored;
    SC_TRY(windowsStat(path, false, ignored));
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::lchmod(StringSpan path, uint32_t mode)
{
    (void)path;
    (void)mode;
    return Result::Error("ENOTSUP");
}

SC::ResultFileSystem SC::FileSystem::Operations::removeEmptyDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeEmptyDirectory: Invalid path");
    SC_TRY_WIN32(::RemoveDirectoryW(path.getNullTerminatedNative()),
                 "removeEmptyDirectory: Failed to remove directory");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::moveDirectory(StringSpan source, StringSpan destination)
{
    SC_TRY_MSG(Internal::validatePath(source), "moveDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destination), "moveDirectory: Invalid destination path");
    SC_TRY_WIN32(::MoveFileExW(source.getNullTerminatedNative(), destination.getNullTerminatedNative(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED),
                 "moveDirectory: Failed to move directory");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeFile(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeFile: Invalid path");
    SC_TRY_WIN32(::DeleteFileW(path.getNullTerminatedNative()), "removeFile: Failed to remove file");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::stat(StringSpan path, FileSystemStat& fileStat)
{
    SC_TRY_MSG(Internal::validatePath(path), "stat: Invalid path");
    return windowsStat(path, true, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::lstat(StringSpan path, FileSystemStat& fileStat)
{
    SC_TRY_MSG(Internal::validatePath(path), "lstat: Invalid path");
    return windowsStat(path, false, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::getFileStat(StringSpan path, FileSystemStat& fileStat)
{
    return stat(path, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::setLastModifiedTime(StringSpan path, TimeMs time)
{
    SC_TRY_MSG(Internal::validatePath(path), "setLastModifiedTime: Invalid path");

    HANDLE hFile = ::CreateFileW(path.getNullTerminatedNative(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return Result::Error("setLastModifiedTime: Failed to open file");
    }
    auto deferClose = MakeDeferred([&]() { CloseHandle(hFile); });

    FILETIME creationTime, lastAccessTime;
    if (!::GetFileTime(hFile, &creationTime, &lastAccessTime, nullptr))
    {
        return Result::Error("setLastModifiedTime: Failed to get file times");
    }

    FILETIME       modifiedTime;
    ULARGE_INTEGER fileTimeValue;
    fileTimeValue.QuadPart      = time.milliseconds * 10000ULL + 116444736000000000ULL;
    modifiedTime.dwLowDateTime  = fileTimeValue.LowPart;
    modifiedTime.dwHighDateTime = fileTimeValue.HighPart;

    SC_TRY_WIN32(::SetFileTime(hFile, &creationTime, &lastAccessTime, &modifiedTime),
                 "setLastModifiedTime: Failed to set file time");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::rename(StringSpan path, StringSpan newPath)
{
    SC_TRY_MSG(Internal::validatePath(path), "rename: Invalid path");
    SC_TRY_MSG(Internal::validatePath(newPath), "rename: Invalid new path");
    SC_TRY_WIN32(::MoveFileW(path.getNullTerminatedNative(), newPath.getNullTerminatedNative()),
                 "rename: Failed to rename");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::copyFile(StringSpan source, StringSpan destination,
                                                          FileSystemCopyFlags flags)
{
    SC_TRY_MSG(Internal::validatePath(source), "copyFile: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destination), "copyFile: Invalid destination path");

    DWORD copyFlags = COPY_FILE_FAIL_IF_EXISTS;
    if (flags.overwrite)
        copyFlags &= ~COPY_FILE_FAIL_IF_EXISTS;

    SC_TRY_WIN32(CopyFileExW(source.getNullTerminatedNative(), destination.getNullTerminatedNative(), nullptr, nullptr,
                             nullptr, copyFlags),
                 "copyFile: Failed to copy file");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::copyDirectory(StringSpan source, StringSpan destination,
                                                               FileSystemCopyFlags flags)
{
    SC_TRY_MSG(Internal::validatePath(source), "copyDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destination), "copyDirectory: Invalid destination path");

    if (flags.overwrite == false and existsAndIsDirectory(destination))
    {
        return Result::Error("copyDirectory: Destination directory already exists");
    }

    return Internal::copyDirectoryRecursive(source.getNullTerminatedNative(), destination.getNullTerminatedNative(),
                                            flags);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeDirectoryRecursive(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeDirectoryRecursive: Invalid path");
    return Internal::removeDirectoryRecursiveInternal(path.getNullTerminatedNative());
}

SC::ResultFileSystem SC::FileSystem::Operations::Internal::copyDirectoryRecursive(const wchar_t*      source,
                                                                                  const wchar_t*      destination,
                                                                                  FileSystemCopyFlags flags)
{
    // Create destination directory if it doesn't exist
    if (::CreateDirectoryW(destination, nullptr) == FALSE)
    {
        if (::GetLastError() != ERROR_ALREADY_EXISTS)
        {
            return Result::Error("copyDirectoryRecursive: Failed to create destination directory");
        }
    }

    // Prepare search pattern
    wchar_t searchPattern[StringPath::MaxPath + 6 + 1] = {};
    if (::swprintf_s(searchPattern, StringPath::MaxPath + 6 + 1, L"%s\\*", source) == -1)
    {
        return Result::Error("copyDirectoryRecursive: Path too long");
    }

    WIN32_FIND_DATAW findData;

    HANDLE hFind = ::FindFirstFileW(searchPattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return Result::Error("copyDirectoryRecursive: Failed to enumerate directory");
    }
    auto deferClose = MakeDeferred([&]() { ::FindClose(hFind); });

    do
    {
        // Skip . and .. entries
        if (::wcscmp(findData.cFileName, L".") == 0 || ::wcscmp(findData.cFileName, L"..") == 0)
            continue;

        // Build full paths
        wchar_t sourcePath[StringPath::MaxPath + 6 + 1] = {};
        wchar_t destPath[StringPath::MaxPath + 6 + 1]   = {};
        if (::swprintf_s(sourcePath, StringPath::MaxPath + 6 + 1, L"%s\\%s", source, findData.cFileName) == -1 ||
            ::swprintf_s(destPath, StringPath::MaxPath + 6 + 1, L"%s\\%s", destination, findData.cFileName) == -1)
        {
            return Result::Error("copyDirectoryRecursive: Path too long");
        }

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Recursively copy subdirectory
            SC_TRY(copyDirectoryRecursive(sourcePath, destPath, flags));
        }
        else
        {
            // Copy file
            DWORD copyFlags = COPY_FILE_FAIL_IF_EXISTS;
            if (flags.overwrite)
                copyFlags &= ~COPY_FILE_FAIL_IF_EXISTS;

            if (::CopyFileExW(sourcePath, destPath, nullptr, nullptr, nullptr, copyFlags) == FALSE)
            {
                return Result::Error("copyDirectoryRecursive: Failed to copy file");
            }
        }
    } while (::FindNextFileW(hFind, &findData));

    if (::GetLastError() != ERROR_NO_MORE_FILES)
    {
        return Result::Error("copyDirectoryRecursive: Failed to enumerate directory");
    }

    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::Internal::removeDirectoryRecursiveInternal(const wchar_t* path)
{
    // Prepare search pattern
    wchar_t searchPattern[StringPath::MaxPath + 6 + 1] = {};
    if (::swprintf_s(searchPattern, StringPath::MaxPath + 6 + 1, L"%s\\*", path) == -1)
    {
        return Result::Error("removeDirectoryRecursive: Path too long");
    }

    WIN32_FIND_DATAW findData;

    HANDLE hFind = ::FindFirstFileW(searchPattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return Result::Error("removeDirectoryRecursive: Failed to enumerate directory");
    }
    auto deferClose = MakeDeferred([&]() { ::FindClose(hFind); });

    do
    {
        // Skip . and .. entries
        if (::wcscmp(findData.cFileName, L".") == 0 || ::wcscmp(findData.cFileName, L"..") == 0)
            continue;

        // Build full path
        wchar_t fullPath[StringPath::MaxPath + 6 + 1] = {};
        if (swprintf_s(fullPath, StringPath::MaxPath + 6 + 1, L"%s\\%s", path, findData.cFileName) == -1)
        {
            return Result::Error("removeDirectoryRecursive: Path too long");
        }

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Recursively remove subdirectory
            SC_TRY(removeDirectoryRecursiveInternal(fullPath));
        }
        else
        {
            // Remove file
            if (::DeleteFileW(fullPath) == FALSE)
            {
                return Result::Error("removeDirectoryRecursive: Failed to delete file");
            }
        }
    } while (::FindNextFileW(hFind, &findData));

    if (::GetLastError() != ERROR_NO_MORE_FILES)
    {
        return Result::Error("removeDirectoryRecursive: Failed to enumerate directory");
    }

    // Remove the now-empty directory
    if (::RemoveDirectoryW(path) == FALSE)
    {
        return Result::Error("removeDirectoryRecursive: Failed to remove directory");
    }

    return Result(true);
}

SC::StringSpan SC::FileSystem::Operations::getExecutablePath(StringPath& executablePath)
{
    if (not FileSystemWindowsDetail::WindowsPath::getExecutablePath(executablePath))
    {
        (void)executablePath.resize(0);
        return {};
    }
    return executablePath.view();
}

SC::StringSpan SC::FileSystem::Operations::getCurrentWorkingDirectory(StringPath& currentWorkingDirectory)
{
    if (not FileSystemWindowsDetail::WindowsPath::getCurrentDirectory(currentWorkingDirectory))
    {
        (void)currentWorkingDirectory.resize(0);
        return {};
    }
    return currentWorkingDirectory.view();
}

SC::StringSpan SC::FileSystem::Operations::getApplicationRootDirectory(StringPath& applicationRootDirectory)
{
    StringSpan exeView = getExecutablePath(applicationRootDirectory);
    if (exeView.isEmpty())
        return {};
    // Find the last path separator (either '\\' or '/')
    ssize_t      lastSeparator = -1;
    wchar_t*     buffer        = applicationRootDirectory.writableSpan().data();
    const size_t bufferLength  = applicationRootDirectory.view().sizeInBytes() / sizeof(wchar_t);
    for (size_t i = 0; i < bufferLength; ++i)
    {
        if (buffer[i] == L'\\' || buffer[i] == L'/')
            lastSeparator = static_cast<ssize_t>(i);
    }
    if (lastSeparator < 0)
    {
        // No separator found, return empty
        (void)applicationRootDirectory.resize(0);
        ::memset(buffer, 0, StringPath::StorageCapacity * sizeof(wchar_t));
        return {};
    }
    const size_t copyLen = static_cast<size_t>(lastSeparator);
    buffer[copyLen]      = 0;
    (void)applicationRootDirectory.resize(copyLen);
    // null terminate the path
    ::memset(buffer + copyLen + 1, 0, (StringPath::StorageCapacity - copyLen - 1) * sizeof(wchar_t));
    return applicationRootDirectory.view();
}
#else

#include <dirent.h>   // DIR, opendir, readdir, closedir
#include <errno.h>    // errno
#include <fcntl.h>    // AT_FDCWD
#include <limits.h>   // PATH_MAX
#include <stdio.h>    // rename
#include <string.h>   // strcmp
#include <sys/stat.h> // mkdir
#include <unistd.h>   // rmdir
#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <copyfile.h>
#include <mach-o/dyld.h> // NSGetExecutablePath
#include <removefile.h>
#include <sys/attr.h>
#include <sys/clonefile.h>
#elif SC_PLATFORM_LINUX
#include <sys/sendfile.h>
#endif
struct SC::FileSystem::Operations::Internal
{
    static ResultFileSystem validatePath(StringSpan path)
    {
        if (path.sizeInBytes() == 0)
            return Result::Error("Path is empty");
        if (path.getEncoding() == StringEncoding::Utf16)
            return Result::Error("Path is not native (UTF8)");
        return Result(true);
    }
    static ResultFileSystem copyFile(StringSpan source, StringSpan destination, FileSystemCopyFlags options,
                                     bool isDirectory = false);
};

static SC::TimeMs posixTimespecToTimeMs(const struct timespec& ts)
{
    constexpr SC::int64_t nanosecondsToMilliseconds = 1000 * 1000;
    constexpr SC::int64_t roundingNanoseconds       = nanosecondsToMilliseconds / 2;
    return SC::TimeMs{static_cast<SC::int64_t>(ts.tv_sec) * 1000 +
                      (static_cast<SC::int64_t>(ts.tv_nsec) + roundingNanoseconds) / nanosecondsToMilliseconds};
}

static SC::FileSystemEntryType posixEntryTypeFromMode(mode_t mode)
{
    if (S_ISREG(mode))
        return SC::FileSystemEntryType::File;
    if (S_ISDIR(mode))
        return SC::FileSystemEntryType::Directory;
    if (S_ISLNK(mode))
        return SC::FileSystemEntryType::SymbolicLink;
    return SC::FileSystemEntryType::Other;
}

static SC::Result fillPosixFileStat(const struct stat& pathStat, SC::FileSystemStat& fileStat)
{
    fileStat               = {};
    fileStat.entryType     = posixEntryTypeFromMode(pathStat.st_mode);
    fileStat.fileSize      = static_cast<size_t>(pathStat.st_size);
    fileStat.hardLinkCount = static_cast<size_t>(pathStat.st_nlink);
    fileStat.accessedTime  = posixTimespecToTimeMs(
#if __APPLE__
        pathStat.st_atimespec
#else
        pathStat.st_atim
#endif
    );
    fileStat.modifiedTime = posixTimespecToTimeMs(
#if __APPLE__
        pathStat.st_mtimespec
#else
        pathStat.st_mtim
#endif
    );
#if __APPLE__
    fileStat.creationTime = posixTimespecToTimeMs(pathStat.st_birthtimespec);
#endif
    fileStat.posix.mode          = static_cast<SC::uint32_t>(pathStat.st_mode);
    fileStat.posix.uid           = static_cast<SC::uint32_t>(pathStat.st_uid);
    fileStat.posix.gid           = static_cast<SC::uint32_t>(pathStat.st_gid);
    fileStat.posix.inode         = static_cast<SC::uint64_t>(pathStat.st_ino);
    fileStat.posix.device        = static_cast<SC::uint64_t>(pathStat.st_dev);
    fileStat.posix.specialDevice = static_cast<SC::uint64_t>(pathStat.st_rdev);
    fileStat.posix.blocks        = static_cast<SC::uint64_t>(pathStat.st_blocks);
    fileStat.posix.blockSize     = static_cast<SC::uint64_t>(pathStat.st_blksize);
    return SC::Result(true);
}

#define SC_TRY_POSIX(func, msg)                                                                                        \
    {                                                                                                                  \
                                                                                                                       \
        if (func != 0)                                                                                                 \
        {                                                                                                              \
            return Result::Error(msg);                                                                                 \
        }                                                                                                              \
    }

SC::ResultFileSystem SC::FileSystem::Operations::createSymbolicLink(StringSpan sourceFileOrDirectory,
                                                                    StringSpan linkFile)
{
    SC_TRY_MSG(Internal::validatePath(sourceFileOrDirectory),
               "createSymbolicLink: Invalid source file or directory path");
    SC_TRY_MSG(Internal::validatePath(linkFile), "createSymbolicLink: Invalid link file path");
    SC_TRY_POSIX(::symlink(sourceFileOrDirectory.getNullTerminatedNative(), linkFile.getNullTerminatedNative()),
                 "createSymbolicLink: Failed to create symbolic link");
    return Result(true);
}

static int posixAccessMode(SC::FileSystemAccessMode accessMode)
{
    switch (accessMode)
    {
    case SC::FileSystemAccessMode::Exists: return F_OK;
    case SC::FileSystemAccessMode::Read: return R_OK;
    case SC::FileSystemAccessMode::Write: return W_OK;
    case SC::FileSystemAccessMode::Execute: return X_OK;
    }
    return F_OK;
}

SC::ResultFileSystem SC::FileSystem::Operations::createHardLink(StringSpan sourceFile, StringSpan linkFile)
{
    SC_TRY_MSG(Internal::validatePath(sourceFile), "createHardLink: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(linkFile), "createHardLink: Invalid link path");
    SC_TRY_POSIX(::link(sourceFile.getNullTerminatedNative(), linkFile.getNullTerminatedNative()),
                 "createHardLink: Failed to create hard link");
    return Result(true);
}

SC::Result SC::FileSystem::Operations::access(StringSpan path, AccessMode accessMode)
{
    SC_TRY_MSG(Internal::validatePath(path), "access: Invalid path");
    return Result(::access(path.getNullTerminatedNative(), posixAccessMode(accessMode)) == 0);
}

SC::ResultFileSystem SC::FileSystem::Operations::makeDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "makeDirectory: Invalid path");
    SC_TRY_POSIX(::mkdir(path.getNullTerminatedNative(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH),
                 "makeDirectory: Failed to create directory");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::makeDirectoryRecursive(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "makeDirectoryRecursive: Invalid path");
    const size_t pathLength = path.sizeInBytes();
    char         temp[PATH_MAX];
    // Copy path to temp, ensure null-terminated
    if (pathLength >= PATH_MAX)
        return Result::Error("makeDirectoryRecursive: Path too long");
    ::memcpy(temp, path.bytesWithoutTerminator(), pathLength);
    // Iterate and create directories
    for (size_t idx = 0; idx < pathLength; ++idx)
    {
        if (temp[idx] == '/' or temp[idx] == '\\')
        {
            if (idx == 0)
                continue; // Skip root
            char old  = temp[idx];
            temp[idx] = 0;
            if (temp[0] != 0) // skip empty
            {
                if (::mkdir(temp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
                {
                    if (errno != EEXIST)
                        return Result::Error("makeDirectoryRecursive: Failed to create parent directory");
                }
            }
            temp[idx] = old;
        }
    }
    // Create the final directory
    if (::mkdir(path.getNullTerminatedNative(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
    {
        if (errno != EEXIST)
            return Result::Error("makeDirectoryRecursive: Failed to create directory");
    }
    return Result(true);
}

SC::Result SC::FileSystem::Operations::exists(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "exists: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "exists: Failed to get file stats");
    return Result(true);
}

SC::Result SC::FileSystem::Operations::existsAndIsDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsDirectory: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "existsAndIsDirectory: Failed to get file stats");
    return Result(S_ISDIR(path_stat.st_mode));
}

SC::Result SC::FileSystem::Operations::existsAndIsFile(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsFile: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "existsAndIsFile: Failed to get file stats");
    return Result(S_ISREG(path_stat.st_mode));
}

SC::Result SC::FileSystem::Operations::existsAndIsLink(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsLink: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::lstat(path.getNullTerminatedNative(), &path_stat), "existsAndIsLink: Failed to get file stats");
    return Result(S_ISLNK(path_stat.st_mode));
}

SC::ResultFileSystem SC::FileSystem::Operations::removeEmptyDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeEmptyDirectory: Invalid path");
    SC_TRY_POSIX(::rmdir(path.getNullTerminatedNative()), "removeEmptyDirectory: Failed to remove directory");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::moveDirectory(StringSpan source, StringSpan destination)
{
    SC_TRY_MSG(Internal::validatePath(source), "moveDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destination), "moveDirectory: Invalid destination path");
    SC_TRY_POSIX(::rename(source.getNullTerminatedNative(), destination.getNullTerminatedNative()),
                 "moveDirectory: Failed to move directory");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeFile(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeFile: Invalid path");
    SC_TRY_POSIX(::remove(path.getNullTerminatedNative()), "removeFile: Failed to remove file");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::stat(StringSpan path, FileSystemStat& fileStat)
{
    SC_TRY_MSG(Internal::validatePath(path), "stat: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "stat: Failed to get file stats");
    return fillPosixFileStat(path_stat, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::lstat(StringSpan path, FileSystemStat& fileStat)
{
    SC_TRY_MSG(Internal::validatePath(path), "lstat: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::lstat(path.getNullTerminatedNative(), &path_stat), "lstat: Failed to get file stats");
    return fillPosixFileStat(path_stat, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::getFileStat(StringSpan path, FileSystemStat& fileStat)
{
    return stat(path, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::readSymbolicLink(StringSpan path, StringPath& destination)
{
    SC_TRY_MSG(Internal::validatePath(path), "readSymbolicLink: Invalid path");

    const ssize_t length =
        ::readlink(path.getNullTerminatedNative(), destination.writableSpan().data(), StringPath::MaxPath - 1);
    if (length < 0)
    {
        return Result::Error("readSymbolicLink: Failed to read link target");
    }

    destination.writableSpan().data()[length] = 0;
    if (not destination.resize(static_cast<size_t>(length)))
    {
        return Result::Error("readSymbolicLink: Failed to store link target");
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::chmod(StringSpan path, uint32_t mode)
{
    SC_TRY_MSG(Internal::validatePath(path), "chmod: Invalid path");
    SC_TRY_POSIX(::chmod(path.getNullTerminatedNative(), static_cast<mode_t>(mode)), "chmod: Failed to change mode");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::chown(StringSpan path, uint32_t uid, uint32_t gid)
{
    SC_TRY_MSG(Internal::validatePath(path), "chown: Invalid path");
    SC_TRY_POSIX(::chown(path.getNullTerminatedNative(), static_cast<uid_t>(uid), static_cast<gid_t>(gid)),
                 "chown: Failed to change owner");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::lchown(StringSpan path, uint32_t uid, uint32_t gid)
{
    SC_TRY_MSG(Internal::validatePath(path), "lchown: Invalid path");
    SC_TRY_POSIX(::lchown(path.getNullTerminatedNative(), static_cast<uid_t>(uid), static_cast<gid_t>(gid)),
                 "lchown: Failed to change owner");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::lchmod(StringSpan path, uint32_t mode)
{
    SC_TRY_MSG(Internal::validatePath(path), "lchmod: Invalid path");
#if SC_PLATFORM_APPLE
    SC_TRY_POSIX(::lchmod(path.getNullTerminatedNative(), static_cast<mode_t>(mode)), "lchmod: Failed to change mode");
    return Result(true);
#else
    (void)mode;
    return Result::Error("ENOTSUP");
#endif
}

SC::ResultFileSystem SC::FileSystem::Operations::setLastModifiedTime(StringSpan path, TimeMs time)
{
    SC_TRY_MSG(Internal::validatePath(path), "setLastModifiedTime: Invalid path");
    struct timespec times[2];
    times[0].tv_sec  = time.milliseconds / 1000;
    times[0].tv_nsec = (time.milliseconds % 1000) * 1000 * 1000;
    times[1]         = times[0];

    SC_TRY_POSIX(::utimensat(AT_FDCWD, path.getNullTerminatedNative(), times, 0),
                 "setLastModifiedTime: Failed to set last modified time");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::rename(StringSpan path, StringSpan newPath)
{
    SC_TRY_MSG(Internal::validatePath(path), "rename: Invalid path");
    SC_TRY_MSG(Internal::validatePath(newPath), "rename: Invalid new path");
    SC_TRY_POSIX(::rename(path.getNullTerminatedNative(), newPath.getNullTerminatedNative()),
                 "rename: Failed to rename");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::copyFile(StringSpan srcPath, StringSpan destPath,
                                                          FileSystemCopyFlags flags)
{
    SC_TRY_MSG(Internal::validatePath(srcPath), "copyFile: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destPath), "copyFile: Invalid destination path");

    return Result(Internal::copyFile(srcPath, destPath, flags, false));
}

SC::ResultFileSystem SC::FileSystem::Operations::copyDirectory(StringSpan srcPath, StringSpan destPath,
                                                               FileSystemCopyFlags flags)
{
    SC_TRY_MSG(Internal::validatePath(srcPath), "copyDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destPath), "copyDirectory: Invalid destination path");
    return Result(Internal::copyFile(srcPath, destPath, flags, true));
}

#if __APPLE__
SC::ResultFileSystem SC::FileSystem::Operations::Internal::copyFile(StringSpan source, StringSpan destination,
                                                                    FileSystemCopyFlags options, bool isDirectory)
{
    const char* sourceFile      = source.getNullTerminatedNative();
    const char* destinationFile = destination.getNullTerminatedNative();

    // Try clonefile and fallback to copyfile in case it fails with ENOTSUP or EXDEV
    // https://www.manpagez.com/man/2/clonefile/
    // https://www.manpagez.com/man/3/copyfile/
    if (options.useCloneIfSupported)
    {
        int cloneRes = ::clonefile(sourceFile, destinationFile, CLONE_NOFOLLOW | CLONE_NOOWNERCOPY);
        if (cloneRes != 0)
        {
            if ((errno == EEXIST) and options.overwrite)
            {
                // TODO: We should probably renaming instead of deleting...and eventually rollback on failure
                if (isDirectory)
                {
                    auto removeState = ::removefile_state_alloc();
                    auto removeFree  = MakeDeferred([&] { ::removefile_state_free(removeState); });
                    SC_TRY_POSIX(::removefile(destinationFile, removeState, REMOVEFILE_RECURSIVE),
                                 "copyFile: Failed to remove file (removeRes == 0)");
                }
                else
                {
                    SC_TRY_POSIX(::remove(destinationFile), "copyFile: Failed to remove file");
                }
                cloneRes = ::clonefile(sourceFile, destinationFile, CLONE_NOFOLLOW | CLONE_NOOWNERCOPY);
            }
        }
        if (cloneRes == 0)
        {
            return Result(true);
        }
        else if (errno != ENOTSUP and errno != EXDEV)
        {
            // We only fallback in case of ENOTSUP and EXDEV (cross-device link)
            return Result::Error("copyFile: Failed to clone file (errno != ENOTSUP and errno != EXDEV)");
        }
    }

    uint32_t flags = COPYFILE_ALL; // We should not use COPYFILE_CLONE_FORCE as clonefile just failed
    if (options.overwrite)
    {
        flags |= COPYFILE_UNLINK;
    }
    if (isDirectory)
    {
        flags |= COPYFILE_RECURSIVE;
    }
    // TODO: Should define flags to decide if to follow symlinks on source and destination
    auto copyState = ::copyfile_state_alloc();
    auto copyFree  = MakeDeferred([&] { ::copyfile_state_free(copyState); });
    SC_TRY_POSIX(::copyfile(sourceFile, destinationFile, copyState, flags), "copyFile: Failed to copy file");
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeDirectoryRecursive(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeDirectoryRecursive: Invalid path");
    auto state     = ::removefile_state_alloc();
    auto stateFree = MakeDeferred([&] { ::removefile_state_free(state); });
    SC_TRY_POSIX(::removefile(path.getNullTerminatedNative(), state, REMOVEFILE_RECURSIVE),
                 "removeDirectoryRecursive: Failed to remove directory");
    return Result(true);
}
#if SC_XCTEST
#include "../Memory/Memory.h" // OPTIONAL DEPENDENCY
#include "Path.h"             // OPTIONAL DEPENDENCY
#include <dlfcn.h>
#endif
SC::StringSpan SC::FileSystem::Operations::getExecutablePath(StringPath& executablePath)
{
#if SC_XCTEST
    Dl_info dlinfo;
    int     res = dladdr((void*)Memory::allocate, &dlinfo);
    if (res != 0 and executablePath.assign(StringSpan::fromNullTerminated(dlinfo.dli_fname, StringEncoding::Utf8)))
    {
        return executablePath.view();
    }
#else
    uint32_t executableLength = static_cast<uint32_t>(StringPath::MaxPath);
    if (::_NSGetExecutablePath(executablePath.writableSpan().data(), &executableLength) == 0)
    {
        (void)executablePath.resize(::strlen(executablePath.view().bytesIncludingTerminator()));
        return executablePath.view();
    }
#endif
    return {};
}

SC::StringSpan SC::FileSystem::Operations::getCurrentWorkingDirectory(StringPath& currentWorkingDirectory)
{
    if (::getcwd(currentWorkingDirectory.writableSpan().data(), StringPath::MaxPath) != nullptr)
    {
        (void)currentWorkingDirectory.resize(::strlen(currentWorkingDirectory.view().bytesIncludingTerminator()));
        return currentWorkingDirectory.view();
    }
    return {};
}

SC::StringSpan SC::FileSystem::Operations::getApplicationRootDirectory(StringPath& applicationRootDirectory)
{
#if SC_XCTEST
    StringView appDir = getExecutablePath(applicationRootDirectory);
    if (not appDir.isEmpty())
    {
        if (applicationRootDirectory.assign(Path::dirname(appDir, Path::AsNative, 3)))
        {
            return applicationRootDirectory.view();
        }
    }
#else
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle != nullptr)
    {
        CFURLRef bundleURL = CFBundleCopyBundleURL(mainBundle);
        if (bundleURL != nullptr)
        {
            uint8_t* path = reinterpret_cast<uint8_t*>(applicationRootDirectory.writableSpan().data());
            if (CFURLGetFileSystemRepresentation(bundleURL, true, path, StringPath::MaxPath))
            {
                (void)applicationRootDirectory.resize(::strlen(reinterpret_cast<char*>(path)));
                CFRelease(bundleURL);
                return applicationRootDirectory.view();
            }
            CFRelease(bundleURL);
        }
    }
#endif
    return {};
}

#else
SC::ResultFileSystem SC::FileSystem::Operations::Internal::copyFile(StringSpan source, StringSpan destination,
                                                                    FileSystemCopyFlags options, bool isDirectory)
{
    if (isDirectory)
    {
        // For directories, first check if source exists and is a directory
        SC_TRY_MSG(existsAndIsDirectory(source), "copyFile: Source path is not a directory");

        // Create destination directory if it doesn't exist
        if (not existsAndIsDirectory(destination))
        {
            SC_TRY(makeDirectory(destination));
        }
        else if (not options.overwrite)
        {
            return Result::Error("copyFile: Destination directory already exists and overwrite is not enabled");
        }

        // Open source directory
        DIR* dir = ::opendir(source.getNullTerminatedNative());
        if (dir == nullptr)
        {
            return Result::Error("copyFile: Failed to open source directory");
        }
        auto closeDir = MakeDeferred([&] { ::closedir(dir); });

        // Buffer for full path construction
        char fullSourcePath[PATH_MAX];
        char fullDestPath[PATH_MAX];

        struct dirent* entry;

        // Iterate through directory entries
        while ((entry = ::readdir(dir)) != nullptr)
        {
            // Skip . and .. entries
            if (::strcmp(entry->d_name, ".") == 0 or ::strcmp(entry->d_name, "..") == 0)
                continue;

            // Construct full paths
            if (::snprintf(fullSourcePath, sizeof(fullSourcePath), "%s/%s", source.getNullTerminatedNative(),
                           entry->d_name) >= static_cast<int>(sizeof(fullSourcePath)) or
                ::snprintf(fullDestPath, sizeof(fullDestPath), "%s/%s", destination.getNullTerminatedNative(),
                           entry->d_name) >= static_cast<int>(sizeof(fullDestPath)))
            {
                return Result::Error("copyFile: Path too long");
            }

            struct stat statbuf;
            if (::lstat(fullSourcePath, &statbuf) != 0)
            {
                return Result::Error("copyFile: Failed to get file stats");
            }

            // Recursively copy subdirectories and files
            SC_TRY(copyFile(StringSpan(fullSourcePath), StringSpan(fullDestPath), options, S_ISDIR(statbuf.st_mode)));
        }

        return Result(true);
    }

    // Original file copying logic for non-directory files
    if (not options.overwrite and existsAndIsFile(destination))
    {
        return Result::Error("copyFile: Failed to copy file (destination file already exists)");
    }
    int inputDescriptor = ::open(source.getNullTerminatedNative(), O_RDONLY);
    if (inputDescriptor < 0)
    {
        return Result::Error("copyFile: Failed to open source file");
    }
    auto closeInput = MakeDeferred([&] { ::close(inputDescriptor); });

    struct stat inputStat;

    SC_TRY_POSIX(::fstat(inputDescriptor, &inputStat), "copyFile: Failed to get file stats");

    int outputDescriptor = ::open(destination.getNullTerminatedNative(), O_WRONLY | O_CREAT | O_TRUNC,
                                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (outputDescriptor < 0)
    {
        return Result::Error("copyFile: Failed to open destination file");
    }
    auto closeOutput = MakeDeferred([&] { ::close(outputDescriptor); });

    const int sendRes = ::sendfile(outputDescriptor, inputDescriptor, nullptr, inputStat.st_size);
    if (sendRes < 0)
    {
        // Sendfile failed, fallback to traditional read/write
        constexpr size_t bufferSize = 4096;

        char buffer[bufferSize];

        ssize_t bytesRead;
        while ((bytesRead = ::read(inputDescriptor, buffer, bufferSize)) > 0)
        {
            if (::write(outputDescriptor, buffer, static_cast<size_t>(bytesRead)) < 0)
            {
                return Result::Error("copyFile: Failed to write to destination file");
            }
        }

        if (bytesRead < 0)
        {
            return Result::Error("copyFile: Failed to read from source file");
        }
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeDirectoryRecursive(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeDirectoryRecursive: Invalid path");

    // Open directory
    DIR* dir = ::opendir(path.getNullTerminatedNative());
    if (dir == nullptr)
    {
        return Result::Error("removeDirectoryRecursive: Failed to open directory");
    }
    auto closeDir = MakeDeferred([&] { ::closedir(dir); });

    // Buffer for full path construction
    char fullPath[PATH_MAX];

    struct dirent* entry;

    // Iterate through directory entries
    while ((entry = ::readdir(dir)) != nullptr)
    {
        // Skip . and .. entries
        if (::strcmp(entry->d_name, ".") == 0 or ::strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full path
        if (::snprintf(fullPath, sizeof(fullPath), "%s/%s", path.getNullTerminatedNative(), entry->d_name) >=
            static_cast<int>(sizeof(fullPath)))
        {
            return Result::Error("removeDirectoryRecursive: Path too long");
        }

        struct stat statbuf;
        if (::lstat(fullPath, &statbuf) != 0)
        {
            return Result::Error("removeDirectoryRecursive: Failed to get file stats");
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            // Recursively remove subdirectory
            SC_TRY(removeDirectoryRecursive(fullPath));
        }
        else
        {
            // Remove file
            if (::unlink(fullPath) != 0)
            {
                return Result::Error("removeDirectoryRecursive: Failed to remove file");
            }
        }
    }

    // Remove the now-empty directory
    if (::rmdir(path.getNullTerminatedNative()) != 0)
    {
        return Result::Error("removeDirectoryRecursive: Failed to remove directory");
    }

    return Result(true);
}

SC::StringSpan SC::FileSystem::Operations::getExecutablePath(StringPath& executablePath)
{
    const int pathLength = ::readlink("/proc/self/exe", executablePath.writableSpan().data(), StringPath::MaxPath);
    if (pathLength > 0)
    {
        (void)executablePath.resize(static_cast<size_t>(pathLength));
        return executablePath.view();
    }
    return {};
}

SC::StringSpan SC::FileSystem::Operations::getCurrentWorkingDirectory(StringPath& currentWorkingDirectory)
{
    if (::getcwd(currentWorkingDirectory.writableSpan().data(), StringPath::MaxPath) != nullptr)
    {
        (void)currentWorkingDirectory.resize(::strlen(currentWorkingDirectory.view().bytesIncludingTerminator()));
        return currentWorkingDirectory.view();
    }
    return {};
}

SC::StringSpan SC::FileSystem::Operations::getApplicationRootDirectory(StringPath& applicationRootDirectory)
{
    StringSpan executablePath = getExecutablePath(applicationRootDirectory);
    if (!executablePath.isEmpty())
    {
        // Get the directory part of the executable path
        char*       buffer    = applicationRootDirectory.writableSpan().data();
        const char* lastSlash = ::strrchr(buffer, '/');
        if (lastSlash != nullptr)
        {
            const size_t newSize = static_cast<size_t>(lastSlash - buffer);
            // Null-terminate the path
            ::memset(buffer + newSize, 0, StringPath::MaxPath - newSize);
            (void)applicationRootDirectory.resize(newSize);
            return applicationRootDirectory.view();
        }
    }
    return {};
}

#endif

#endif
