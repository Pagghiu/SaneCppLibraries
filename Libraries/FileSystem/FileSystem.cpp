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

static bool isMissingWindowsError(SC::uint32_t nativeError)
{
    return nativeError == ERROR_FILE_NOT_FOUND or nativeError == ERROR_PATH_NOT_FOUND;
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

static bool isMissingPosixError(int nativeError) { return nativeError == ENOENT or nativeError == ENOTDIR; }
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
    bool isDirectory;
    SC_TRY(existsAndIsDirectory(".", isDirectory));
    if (not isDirectory)
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
    bool isFile;
    SC_TRY(existsAndIsFile(source, isFile));
    if (isFile)
        return removeFiles(Span<const StringSpan>{source});
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::removeLinkIfExists(StringSpan source)
{
    bool isLink;
    SC_TRY(existsAndIsLink(source, isLink));
    if (isLink)
    {
#if SC_PLATFORM_WINDOWS
        bool isDirectory;
        SC_TRY(existsAndIsDirectory(source, isDirectory));
        if (isDirectory)
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
        return {FileSystemError::NotInitialized, FileSystemErrorDetail::BuildTransportPath};
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
        return {FileSystemError::NotInitialized, FileSystemErrorDetail::BuildTransportPath};
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
        bool isDirectory;
        SC_TRY(existsAndIsDirectory(path, isDirectory));
        if (not isDirectory)
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
    bool doesExist = false;
    return exists(fileOrDirectory, doesExist) and doesExist;
}

SC::ResultFileSystem SC::FileSystem::exists(StringSpan fileOrDirectory, bool& doesExist)
{
    doesExist = false;
    StringSpan encodedPath;
    SC_TRY(convert(fileOrDirectory, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    return FileSystem::Operations::exists(encodedPath, doesExist);
}

bool SC::FileSystem::existsAndIsDirectory(StringSpan directory)
{
    bool isDirectory = false;
    return existsAndIsDirectory(directory, isDirectory) and isDirectory;
}

SC::ResultFileSystem SC::FileSystem::existsAndIsDirectory(StringSpan directory, bool& isDirectory)
{
    isDirectory = false;
    StringSpan encodedPath;
    SC_TRY(convert(directory, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    return FileSystem::Operations::existsAndIsDirectory(encodedPath, isDirectory);
}

bool SC::FileSystem::existsAndIsFile(StringSpan file)
{
    bool isFile = false;
    return existsAndIsFile(file, isFile) and isFile;
}

SC::ResultFileSystem SC::FileSystem::existsAndIsFile(StringSpan file, bool& isFile)
{
    isFile = false;
    StringSpan encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    return FileSystem::Operations::existsAndIsFile(encodedPath, isFile);
}

bool SC::FileSystem::existsAndIsLink(StringSpan file)
{
    bool isLink = false;
    return existsAndIsLink(file, isLink) and isLink;
}

SC::ResultFileSystem SC::FileSystem::existsAndIsLink(StringSpan file, bool& isLink)
{
    isLink = false;
    StringSpan encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    return FileSystem::Operations::existsAndIsLink(encodedPath, isLink);
}

bool SC::FileSystem::canAccess(StringSpan fileOrDirectory, AccessMode accessMode)
{
    bool canAccessPath = false;
    return canAccess(fileOrDirectory, accessMode, canAccessPath) and canAccessPath;
}

SC::ResultFileSystem SC::FileSystem::canAccess(StringSpan fileOrDirectory, AccessMode accessMode, bool& canAccessPath)
{
    canAccessPath = false;
    StringSpan encodedPath;
    SC_TRY(convert(fileOrDirectory, fileFormatBuffer1, fileTransportBuffer1, &encodedPath));
    return FileSystem::Operations::access(encodedPath, accessMode, canAccessPath);
}

SC::ResultFileSystem SC::FileSystem::moveDirectory(StringSpan sourceDirectory, StringSpan destinationDirectory)
{
    StringSpan encodedPath1;
    StringSpan encodedPath2;
    SC_TRY(convert(sourceDirectory, fileFormatBuffer1, fileTransportBuffer1, &encodedPath1));
    SC_TRY(convert(destinationDirectory, fileFormatBuffer2, fileTransportBuffer2, &encodedPath2));
    return FileSystem::Operations::moveDirectory(encodedPath1, encodedPath2);
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
            return {FileSystemError::InvalidPath, FileSystemErrorDetail::NormalizePath};
        if (path.getEncoding() != StringEncoding::Utf16)
            return {FileSystemError::UnsupportedPathEncoding, FileSystemErrorDetail::ValidatePathEncoding};
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

static SC::ResultFileSystem fillWindowsFileStat(HANDLE hFile, SC::FileSystemStat& fileStat)
{
    fileStat = {};

    BY_HANDLE_FILE_INFORMATION handleInfo;
    if (::GetFileInformationByHandle(hFile, &handleInfo) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), SC::FileSystemErrorDetail::QueryEntryMetadata);
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

static SC::ResultFileSystem windowsStat(SC::StringSpan path, bool followLinks, SC::FileSystemStat& fileStat)
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
        return fileSystemResultFromNative(::GetLastError(), SC::FileSystemErrorDetail::QueryEntryMetadata);
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

SC::ResultFileSystem SC::FileSystem::Operations::createSymbolicLink(StringSpan sourceFileOrDirectory,
                                                                    StringSpan linkFile)
{
    SC_TRY(Internal::validatePath(sourceFileOrDirectory));
    SC_TRY(Internal::validatePath(linkFile));

    bool isDirectory;
    SC_TRY(existsAndIsDirectory(sourceFileOrDirectory, isDirectory));
    DWORD dwFlags = isDirectory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
    dwFlags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    if (::CreateSymbolicLinkW(linkFile.getNullTerminatedNative(), sourceFileOrDirectory.getNullTerminatedNative(),
                              dwFlags) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::CreateSymbolicLinkEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::createHardLink(StringSpan sourceFile, StringSpan linkFile)
{
    SC_TRY(Internal::validatePath(sourceFile));
    SC_TRY(Internal::validatePath(linkFile));
    if (::CreateHardLinkW(linkFile.getNullTerminatedNative(), sourceFile.getNullTerminatedNative(), nullptr) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::CreateHardLinkEntry);
    return Result(true);
}

bool SC::FileSystem::Operations::access(StringSpan path, AccessMode accessMode)
{
    bool canAccessPath = false;
    return access(path, accessMode, canAccessPath) and canAccessPath;
}

SC::ResultFileSystem SC::FileSystem::Operations::access(StringSpan path, AccessMode accessMode, bool& canAccessPath)
{
    canAccessPath = false;
    SC_TRY(Internal::validatePath(path));

    int mode = 0;
    switch (accessMode)
    {
    case AccessMode::Exists: mode = 0; break;
    case AccessMode::Read: mode = 4; break;
    case AccessMode::Write: mode = 2; break;
    case AccessMode::Execute: mode = 0; break;
    }
    if (::_waccess(path.getNullTerminatedNative(), mode) == 0)
    {
        canAccessPath = true;
        return Result(true);
    }
    const int nativeError = errno;
    if (nativeError == EACCES or nativeError == ENOENT)
        return Result(true);
    if (nativeError == EINVAL)
        return ResultFileSystem::withNativeError(FileSystemError::InvalidArgument, FileSystemErrorDetail::CheckAccess,
                                                 static_cast<uint32_t>(nativeError));
    return ResultFileSystem::withNativeError(FileSystemError::OperationFailed, FileSystemErrorDetail::CheckAccess,
                                             static_cast<uint32_t>(nativeError));
}

SC::ResultFileSystem SC::FileSystem::Operations::makeDirectory(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));
    if (::CreateDirectoryW(path.getNullTerminatedNative(), nullptr) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::CreateDirectoryEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::makeDirectoryRecursive(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));
    const size_t pathLength = path.sizeInBytes() / sizeof(wchar_t);
    if (pathLength < 2)
        return {FileSystemError::InvalidPath, FileSystemErrorDetail::CreateDirectoryEntry};
    wchar_t temp[StringPath::MaxPath + 6 + 1] = {};
    // Copy path to temp, ensure null-terminated
    if (pathLength > StringPath::MaxPath + 6)
        return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::CreateDirectoryEntry};
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
                    const DWORD nativeError = ::GetLastError();
                    if (nativeError != ERROR_ALREADY_EXISTS)
                        return fileSystemResultFromNative(nativeError, FileSystemErrorDetail::CreateParentDirectory);
                }
            }
            temp[idx] = old;
        }
    }
    // Create the final directory
    if (!::CreateDirectoryW(temp, nullptr))
    {
        const DWORD nativeError = ::GetLastError();
        if (nativeError != ERROR_ALREADY_EXISTS)
            return fileSystemResultFromNative(nativeError, FileSystemErrorDetail::CreateDirectoryEntry);
    }
    return Result(true);
}

bool SC::FileSystem::Operations::exists(StringSpan path)
{
    bool doesExist = false;
    return exists(path, doesExist) and doesExist;
}

SC::ResultFileSystem SC::FileSystem::Operations::exists(StringSpan path, bool& doesExist)
{
    doesExist = false;
    SC_TRY(Internal::validatePath(path));
    if (::GetFileAttributesW(path.getNullTerminatedNative()) != INVALID_FILE_ATTRIBUTES)
    {
        doesExist = true;
        return Result(true);
    }
    const DWORD nativeError = ::GetLastError();
    if (isMissingWindowsError(nativeError))
        return Result(true);
    return fileSystemResultFromNative(nativeError, FileSystemErrorDetail::QueryEntryMetadata);
}

bool SC::FileSystem::Operations::existsAndIsDirectory(StringSpan path)
{
    bool isDirectory = false;
    return existsAndIsDirectory(path, isDirectory) and isDirectory;
}

SC::ResultFileSystem SC::FileSystem::Operations::existsAndIsDirectory(StringSpan path, bool& isDirectory)
{
    isDirectory = false;
    SC_TRY(Internal::validatePath(path));
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
    {
        const DWORD nativeError = ::GetLastError();
        if (isMissingWindowsError(nativeError))
            return Result(true);
        return fileSystemResultFromNative(nativeError, FileSystemErrorDetail::QueryEntryMetadata);
    }
    isDirectory = (res & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return Result(true);
}

bool SC::FileSystem::Operations::existsAndIsFile(StringSpan path)
{
    bool isFile = false;
    return existsAndIsFile(path, isFile) and isFile;
}

SC::ResultFileSystem SC::FileSystem::Operations::existsAndIsFile(StringSpan path, bool& isFile)
{
    isFile = false;
    SC_TRY(Internal::validatePath(path));
    const DWORD res = GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
    {
        const DWORD nativeError = ::GetLastError();
        if (isMissingWindowsError(nativeError))
            return Result(true);
        return fileSystemResultFromNative(nativeError, FileSystemErrorDetail::QueryEntryMetadata);
    }
    isFile = (res & FILE_ATTRIBUTE_DIRECTORY) == 0;
    return Result(true);
}

bool SC::FileSystem::Operations::existsAndIsLink(StringSpan path)
{
    bool isLink = false;
    return existsAndIsLink(path, isLink) and isLink;
}

SC::ResultFileSystem SC::FileSystem::Operations::existsAndIsLink(StringSpan path, bool& isLink)
{
    isLink = false;
    SC_TRY(Internal::validatePath(path));
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
    {
        const DWORD nativeError = ::GetLastError();
        if (isMissingWindowsError(nativeError))
            return Result(true);
        return fileSystemResultFromNative(nativeError, FileSystemErrorDetail::QueryEntryMetadata);
    }
    isLink = (res & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::readSymbolicLink(StringSpan path, StringPath& destination)
{
    SC_TRY(Internal::validatePath(path));

    HANDLE hFile =
        ::CreateFileW(path.getNullTerminatedNative(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::ReadSymbolicLinkTarget);
    }
    auto deferClose = MakeDeferred([&]() { ::CloseHandle(hFile); });

    alignas(void*) char reparseStorage[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];

    DWORD bytesReturned = 0;
    if (::DeviceIoControl(hFile, FSCTL_GET_REPARSE_POINT, nullptr, 0, reparseStorage, MAXIMUM_REPARSE_DATA_BUFFER_SIZE,
                          &bytesReturned, nullptr) == FALSE)
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::WindowsQueryReparsePoint);
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
        return {FileSystemError::UnsupportedEntryType, FileSystemErrorDetail::WindowsQueryReparsePoint};
    }

    if (sourceChars > StringPath::MaxPath)
    {
        const uint64_t requiredBytes = static_cast<uint64_t>(sourceChars) * sizeof(wchar_t);
        if (requiredBytes <= 0xffffffffu)
            return ResultFileSystem::withRequiredBytes(FileSystemError::BufferCapacityExceeded,
                                                       FileSystemErrorDetail::StoreSymbolicLinkTarget,
                                                       static_cast<uint32_t>(requiredBytes));
        return {FileSystemError::BufferCapacityExceeded, FileSystemErrorDetail::StoreSymbolicLinkTarget};
    }

    ::memcpy(destination.writableSpan().data(), sourcePath, sourceChars * sizeof(wchar_t));
    destination.writableSpan().data()[sourceChars] = 0;
    if (not destination.resize(sourceChars))
        return {FileSystemError::BufferCapacityExceeded, FileSystemErrorDetail::StoreSymbolicLinkTarget};
    const auto pathResult = FileSystemWindowsDetail::WindowsPath::makeLogicalPath(destination.view(), destination);
    if (not pathResult)
        return translateWindowsPathError(pathResult, FileSystemErrorDetail::NormalizePath);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::chmod(StringSpan path, uint32_t mode)
{
    SC_TRY(Internal::validatePath(path));
    DWORD attributes = ::GetFileAttributesW(path.getNullTerminatedNative());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::QueryEntryMetadata);
    }
    if (mode & windowsWriteModeBit)
    {
        attributes &= ~FILE_ATTRIBUTE_READONLY;
    }
    else
    {
        attributes |= FILE_ATTRIBUTE_READONLY;
    }
    if (::SetFileAttributesW(path.getNullTerminatedNative(), attributes) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::ChangePermissions);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::chown(StringSpan path, uint32_t uid, uint32_t gid)
{
    (void)uid;
    (void)gid;
    SC_TRY(Internal::validatePath(path));
    return {FileSystemError::OperationUnsupported, FileSystemErrorDetail::ChangeOwnership};
}

SC::ResultFileSystem SC::FileSystem::Operations::lchown(StringSpan path, uint32_t uid, uint32_t gid)
{
    (void)uid;
    (void)gid;
    SC_TRY(Internal::validatePath(path));
    return {FileSystemError::OperationUnsupported, FileSystemErrorDetail::ChangeLinkOwnership};
}

SC::ResultFileSystem SC::FileSystem::Operations::lchmod(StringSpan path, uint32_t mode)
{
    (void)mode;
    SC_TRY(Internal::validatePath(path));
    return {FileSystemError::OperationUnsupported, FileSystemErrorDetail::ChangeLinkPermissions};
}

SC::ResultFileSystem SC::FileSystem::Operations::removeEmptyDirectory(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));
    if (::RemoveDirectoryW(path.getNullTerminatedNative()) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::RemoveDirectoryEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::moveDirectory(StringSpan source, StringSpan destination)
{
    SC_TRY(Internal::validatePath(source));
    SC_TRY(Internal::validatePath(destination));
    if (::MoveFileExW(source.getNullTerminatedNative(), destination.getNullTerminatedNative(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::RenameEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeFile(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));
    if (::DeleteFileW(path.getNullTerminatedNative()) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::RemoveFileEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::stat(StringSpan path, FileSystemStat& fileStat)
{
    SC_TRY(Internal::validatePath(path));
    return windowsStat(path, true, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::lstat(StringSpan path, FileSystemStat& fileStat)
{
    SC_TRY(Internal::validatePath(path));
    return windowsStat(path, false, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::getFileStat(StringSpan path, FileSystemStat& fileStat)
{
    return stat(path, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::setLastModifiedTime(StringSpan path, TimeMs time)
{
    SC_TRY(Internal::validatePath(path));

    HANDLE hFile = ::CreateFileW(path.getNullTerminatedNative(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::ChangeModifiedTime);
    }
    auto deferClose = MakeDeferred([&]() { CloseHandle(hFile); });

    FILETIME creationTime, lastAccessTime;
    if (!::GetFileTime(hFile, &creationTime, &lastAccessTime, nullptr))
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::QueryFileTimes);
    }

    FILETIME       modifiedTime;
    ULARGE_INTEGER fileTimeValue;
    fileTimeValue.QuadPart      = time.milliseconds * 10000ULL + 116444736000000000ULL;
    modifiedTime.dwLowDateTime  = fileTimeValue.LowPart;
    modifiedTime.dwHighDateTime = fileTimeValue.HighPart;

    if (::SetFileTime(hFile, &creationTime, &lastAccessTime, &modifiedTime) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::SetFileTimes);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::rename(StringSpan path, StringSpan newPath)
{
    SC_TRY(Internal::validatePath(path));
    SC_TRY(Internal::validatePath(newPath));
    if (::MoveFileW(path.getNullTerminatedNative(), newPath.getNullTerminatedNative()) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::RenameEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::copyFile(StringSpan source, StringSpan destination,
                                                          FileSystemCopyFlags flags)
{
    SC_TRY(Internal::validatePath(source));
    SC_TRY(Internal::validatePath(destination));

    DWORD copyFlags = COPY_FILE_FAIL_IF_EXISTS;
    if (flags.overwrite)
        copyFlags &= ~COPY_FILE_FAIL_IF_EXISTS;

    if (::CopyFileExW(source.getNullTerminatedNative(), destination.getNullTerminatedNative(), nullptr, nullptr,
                      nullptr, copyFlags) == FALSE)
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::CopyFileEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::copyDirectory(StringSpan source, StringSpan destination,
                                                               FileSystemCopyFlags flags)
{
    SC_TRY(Internal::validatePath(source));
    SC_TRY(Internal::validatePath(destination));

    bool destinationIsDirectory;
    SC_TRY(existsAndIsDirectory(destination, destinationIsDirectory));
    if (flags.overwrite == false and destinationIsDirectory)
    {
        return {FileSystemError::EntryAlreadyExists, FileSystemErrorDetail::CopyDirectoryTree};
    }

    return Internal::copyDirectoryRecursive(source.getNullTerminatedNative(), destination.getNullTerminatedNative(),
                                            flags);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeDirectoryRecursive(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));
    return Internal::removeDirectoryRecursiveInternal(path.getNullTerminatedNative());
}

SC::ResultFileSystem SC::FileSystem::Operations::Internal::copyDirectoryRecursive(const wchar_t*      source,
                                                                                  const wchar_t*      destination,
                                                                                  FileSystemCopyFlags flags)
{
    // Create destination directory if it doesn't exist
    if (::CreateDirectoryW(destination, nullptr) == FALSE)
    {
        const DWORD nativeError = ::GetLastError();
        if (nativeError != ERROR_ALREADY_EXISTS)
            return fileSystemResultFromNative(nativeError, FileSystemErrorDetail::CreateDirectoryEntry);
    }

    // Prepare search pattern
    wchar_t searchPattern[StringPath::MaxPath + 6 + 1] = {};
    if (::swprintf_s(searchPattern, StringPath::MaxPath + 6 + 1, L"%s\\*", source) == -1)
    {
        return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::BuildChildPath};
    }

    WIN32_FIND_DATAW findData;

    HANDLE hFind = ::FindFirstFileW(searchPattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::EnumerateDirectory);
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
            return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::BuildChildPath};
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
                return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::CopyChildEntry);
            }
        }
    } while (::FindNextFileW(hFind, &findData));

    const DWORD enumerationError = ::GetLastError();
    if (enumerationError != ERROR_NO_MORE_FILES)
    {
        return fileSystemResultFromNative(enumerationError, FileSystemErrorDetail::EnumerateDirectory);
    }

    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::Internal::removeDirectoryRecursiveInternal(const wchar_t* path)
{
    // Prepare search pattern
    wchar_t searchPattern[StringPath::MaxPath + 6 + 1] = {};
    if (::swprintf_s(searchPattern, StringPath::MaxPath + 6 + 1, L"%s\\*", path) == -1)
    {
        return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::BuildChildPath};
    }

    WIN32_FIND_DATAW findData;

    HANDLE hFind = ::FindFirstFileW(searchPattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::EnumerateDirectory);
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
            return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::BuildChildPath};
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
                return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::RemoveChildEntry);
            }
        }
    } while (::FindNextFileW(hFind, &findData));

    const DWORD enumerationError = ::GetLastError();
    if (enumerationError != ERROR_NO_MORE_FILES)
    {
        return fileSystemResultFromNative(enumerationError, FileSystemErrorDetail::EnumerateDirectory);
    }

    // Remove the now-empty directory
    if (::RemoveDirectoryW(path) == FALSE)
    {
        return fileSystemResultFromNative(::GetLastError(), FileSystemErrorDetail::RemoveDirectoryEntry);
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
            return {FileSystemError::InvalidPath, FileSystemErrorDetail::NormalizePath};
        if (path.getEncoding() == StringEncoding::Utf16)
            return {FileSystemError::UnsupportedPathEncoding, FileSystemErrorDetail::ValidatePathEncoding};
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

SC::ResultFileSystem SC::FileSystem::Operations::createSymbolicLink(StringSpan sourceFileOrDirectory,
                                                                    StringSpan linkFile)
{
    SC_TRY(Internal::validatePath(sourceFileOrDirectory));
    SC_TRY(Internal::validatePath(linkFile));
    if (::symlink(sourceFileOrDirectory.getNullTerminatedNative(), linkFile.getNullTerminatedNative()) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::CreateSymbolicLinkEntry);
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
    SC_TRY(Internal::validatePath(sourceFile));
    SC_TRY(Internal::validatePath(linkFile));
    if (::link(sourceFile.getNullTerminatedNative(), linkFile.getNullTerminatedNative()) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::CreateHardLinkEntry);
    return Result(true);
}

bool SC::FileSystem::Operations::access(StringSpan path, AccessMode accessMode)
{
    bool canAccessPath = false;
    return access(path, accessMode, canAccessPath) and canAccessPath;
}

SC::ResultFileSystem SC::FileSystem::Operations::access(StringSpan path, AccessMode accessMode, bool& canAccessPath)
{
    canAccessPath = false;
    SC_TRY(Internal::validatePath(path));
    if (::access(path.getNullTerminatedNative(), posixAccessMode(accessMode)) == 0)
    {
        canAccessPath = true;
        return Result(true);
    }
    const int nativeError = errno;
    if (nativeError == EACCES or isMissingPosixError(nativeError))
        return Result(true);
    return fileSystemResultFromNative(static_cast<uint32_t>(nativeError), FileSystemErrorDetail::CheckAccess);
}

SC::ResultFileSystem SC::FileSystem::Operations::makeDirectory(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));
    if (::mkdir(path.getNullTerminatedNative(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::CreateDirectoryEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::makeDirectoryRecursive(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));
    const size_t pathLength = path.sizeInBytes();
    char         temp[PATH_MAX];
    // Copy path to temp, ensure null-terminated
    if (pathLength >= PATH_MAX)
        return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::CreateDirectoryEntry};
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
                    const int nativeError = errno;
                    if (nativeError != EEXIST)
                        return fileSystemResultFromNative(static_cast<uint32_t>(nativeError),
                                                          FileSystemErrorDetail::CreateParentDirectory);
                }
            }
            temp[idx] = old;
        }
    }
    // Create the final directory
    if (::mkdir(path.getNullTerminatedNative(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
    {
        const int nativeError = errno;
        if (nativeError != EEXIST)
            return fileSystemResultFromNative(static_cast<uint32_t>(nativeError),
                                              FileSystemErrorDetail::CreateDirectoryEntry);
    }
    return Result(true);
}

bool SC::FileSystem::Operations::exists(StringSpan path)
{
    bool doesExist = false;
    return exists(path, doesExist) and doesExist;
}

SC::ResultFileSystem SC::FileSystem::Operations::exists(StringSpan path, bool& doesExist)
{
    doesExist = false;
    SC_TRY(Internal::validatePath(path));
    struct stat path_stat;
    if (::stat(path.getNullTerminatedNative(), &path_stat) == 0)
    {
        doesExist = true;
        return Result(true);
    }
    const int nativeError = errno;
    if (isMissingPosixError(nativeError))
        return Result(true);
    return fileSystemResultFromNative(static_cast<uint32_t>(nativeError), FileSystemErrorDetail::QueryEntryMetadata);
}

bool SC::FileSystem::Operations::existsAndIsDirectory(StringSpan path)
{
    bool isDirectory = false;
    return existsAndIsDirectory(path, isDirectory) and isDirectory;
}

SC::ResultFileSystem SC::FileSystem::Operations::existsAndIsDirectory(StringSpan path, bool& isDirectory)
{
    isDirectory = false;
    SC_TRY(Internal::validatePath(path));
    struct stat path_stat;
    if (::stat(path.getNullTerminatedNative(), &path_stat) != 0)
    {
        const int nativeError = errno;
        if (isMissingPosixError(nativeError))
            return Result(true);
        return fileSystemResultFromNative(static_cast<uint32_t>(nativeError),
                                          FileSystemErrorDetail::QueryEntryMetadata);
    }
    isDirectory = S_ISDIR(path_stat.st_mode);
    return Result(true);
}

bool SC::FileSystem::Operations::existsAndIsFile(StringSpan path)
{
    bool isFile = false;
    return existsAndIsFile(path, isFile) and isFile;
}

SC::ResultFileSystem SC::FileSystem::Operations::existsAndIsFile(StringSpan path, bool& isFile)
{
    isFile = false;
    SC_TRY(Internal::validatePath(path));
    struct stat path_stat;
    if (::stat(path.getNullTerminatedNative(), &path_stat) != 0)
    {
        const int nativeError = errno;
        if (isMissingPosixError(nativeError))
            return Result(true);
        return fileSystemResultFromNative(static_cast<uint32_t>(nativeError),
                                          FileSystemErrorDetail::QueryEntryMetadata);
    }
    isFile = S_ISREG(path_stat.st_mode);
    return Result(true);
}

bool SC::FileSystem::Operations::existsAndIsLink(StringSpan path)
{
    bool isLink = false;
    return existsAndIsLink(path, isLink) and isLink;
}

SC::ResultFileSystem SC::FileSystem::Operations::existsAndIsLink(StringSpan path, bool& isLink)
{
    isLink = false;
    SC_TRY(Internal::validatePath(path));
    struct stat path_stat;
    if (::lstat(path.getNullTerminatedNative(), &path_stat) != 0)
    {
        const int nativeError = errno;
        if (isMissingPosixError(nativeError))
            return Result(true);
        return fileSystemResultFromNative(static_cast<uint32_t>(nativeError),
                                          FileSystemErrorDetail::QueryEntryMetadata);
    }
    isLink = S_ISLNK(path_stat.st_mode);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeEmptyDirectory(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));
    if (::rmdir(path.getNullTerminatedNative()) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::RemoveDirectoryEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::moveDirectory(StringSpan source, StringSpan destination)
{
    SC_TRY(Internal::validatePath(source));
    SC_TRY(Internal::validatePath(destination));
    if (::rename(source.getNullTerminatedNative(), destination.getNullTerminatedNative()) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::RenameEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeFile(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));
    if (::remove(path.getNullTerminatedNative()) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::RemoveFileEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::stat(StringSpan path, FileSystemStat& fileStat)
{
    SC_TRY(Internal::validatePath(path));
    struct stat path_stat;
    if (::stat(path.getNullTerminatedNative(), &path_stat) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::QueryEntryMetadata);
    return fillPosixFileStat(path_stat, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::lstat(StringSpan path, FileSystemStat& fileStat)
{
    SC_TRY(Internal::validatePath(path));
    struct stat path_stat;
    if (::lstat(path.getNullTerminatedNative(), &path_stat) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::QueryEntryMetadata);
    return fillPosixFileStat(path_stat, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::getFileStat(StringSpan path, FileSystemStat& fileStat)
{
    return stat(path, fileStat);
}

SC::ResultFileSystem SC::FileSystem::Operations::readSymbolicLink(StringSpan path, StringPath& destination)
{
    SC_TRY(Internal::validatePath(path));

    const ssize_t length =
        ::readlink(path.getNullTerminatedNative(), destination.writableSpan().data(), StringPath::MaxPath - 1);
    if (length < 0)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::ReadSymbolicLinkTarget);
    }

    destination.writableSpan().data()[length] = 0;
    if (not destination.resize(static_cast<size_t>(length)))
    {
        return {FileSystemError::BufferCapacityExceeded, FileSystemErrorDetail::StoreSymbolicLinkTarget};
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::chmod(StringSpan path, uint32_t mode)
{
    SC_TRY(Internal::validatePath(path));
    if (::chmod(path.getNullTerminatedNative(), static_cast<mode_t>(mode)) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::ChangePermissions);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::chown(StringSpan path, uint32_t uid, uint32_t gid)
{
    SC_TRY(Internal::validatePath(path));
    if (::chown(path.getNullTerminatedNative(), static_cast<uid_t>(uid), static_cast<gid_t>(gid)) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::ChangeOwnership);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::lchown(StringSpan path, uint32_t uid, uint32_t gid)
{
    SC_TRY(Internal::validatePath(path));
    if (::lchown(path.getNullTerminatedNative(), static_cast<uid_t>(uid), static_cast<gid_t>(gid)) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::ChangeLinkOwnership);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::lchmod(StringSpan path, uint32_t mode)
{
    SC_TRY(Internal::validatePath(path));
#if SC_PLATFORM_APPLE
    if (::lchmod(path.getNullTerminatedNative(), static_cast<mode_t>(mode)) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::ChangeLinkPermissions);
    return Result(true);
#else
    (void)mode;
    return {FileSystemError::OperationUnsupported, FileSystemErrorDetail::ChangeLinkPermissions};
#endif
}

SC::ResultFileSystem SC::FileSystem::Operations::setLastModifiedTime(StringSpan path, TimeMs time)
{
    SC_TRY(Internal::validatePath(path));
    struct timespec times[2];
    times[0].tv_sec  = time.milliseconds / 1000;
    times[0].tv_nsec = (time.milliseconds % 1000) * 1000 * 1000;
    times[1]         = times[0];

    if (::utimensat(AT_FDCWD, path.getNullTerminatedNative(), times, 0) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::SetFileTimes);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::rename(StringSpan path, StringSpan newPath)
{
    SC_TRY(Internal::validatePath(path));
    SC_TRY(Internal::validatePath(newPath));
    if (::rename(path.getNullTerminatedNative(), newPath.getNullTerminatedNative()) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::RenameEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::copyFile(StringSpan srcPath, StringSpan destPath,
                                                          FileSystemCopyFlags flags)
{
    SC_TRY(Internal::validatePath(srcPath));
    SC_TRY(Internal::validatePath(destPath));

    return Internal::copyFile(srcPath, destPath, flags, false);
}

SC::ResultFileSystem SC::FileSystem::Operations::copyDirectory(StringSpan srcPath, StringSpan destPath,
                                                               FileSystemCopyFlags flags)
{
    SC_TRY(Internal::validatePath(srcPath));
    SC_TRY(Internal::validatePath(destPath));
    return Internal::copyFile(srcPath, destPath, flags, true);
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
                    if (::removefile(destinationFile, removeState, REMOVEFILE_RECURSIVE) != 0)
                        return fileSystemResultFromNative(static_cast<uint32_t>(errno),
                                                          FileSystemErrorDetail::RemoveDirectoryEntry);
                }
                else
                {
                    if (::remove(destinationFile) != 0)
                        return fileSystemResultFromNative(static_cast<uint32_t>(errno),
                                                          FileSystemErrorDetail::RemoveFileEntry);
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
            return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::CloneEntry);
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
    if (::copyfile(sourceFile, destinationFile, copyState, flags) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), isDirectory
                                                                            ? FileSystemErrorDetail::CopyDirectoryTree
                                                                            : FileSystemErrorDetail::CopyFileEntry);
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeDirectoryRecursive(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));
    auto state     = ::removefile_state_alloc();
    auto stateFree = MakeDeferred([&] { ::removefile_state_free(state); });
    if (::removefile(path.getNullTerminatedNative(), state, REMOVEFILE_RECURSIVE) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::RemoveDirectoryEntry);
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
        struct stat sourceStat;
        if (::stat(source.getNullTerminatedNative(), &sourceStat) != 0)
            return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::QueryEntryMetadata);
        if (not S_ISDIR(sourceStat.st_mode))
            return {FileSystemError::EntryTypeMismatch, FileSystemErrorDetail::CopyDirectoryTree};

        // Create destination directory if it doesn't exist
        struct stat destinationStat;
        if (::stat(destination.getNullTerminatedNative(), &destinationStat) != 0)
        {
            const int nativeError = errno;
            if (nativeError != ENOENT)
                return fileSystemResultFromNative(static_cast<uint32_t>(nativeError),
                                                  FileSystemErrorDetail::QueryEntryMetadata);
            if (::mkdir(destination.getNullTerminatedNative(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
                return fileSystemResultFromNative(static_cast<uint32_t>(errno),
                                                  FileSystemErrorDetail::CreateDirectoryEntry);
        }
        else if (not S_ISDIR(destinationStat.st_mode))
        {
            return {FileSystemError::EntryTypeMismatch, FileSystemErrorDetail::CopyDirectoryTree};
        }
        else if (not options.overwrite)
        {
            return {FileSystemError::EntryAlreadyExists, FileSystemErrorDetail::CopyDirectoryTree};
        }

        // Open source directory
        DIR* dir = ::opendir(source.getNullTerminatedNative());
        if (dir == nullptr)
        {
            return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::OpenSourceDirectory);
        }
        auto closeDir = MakeDeferred([&] { ::closedir(dir); });

        // Buffer for full path construction
        char fullSourcePath[PATH_MAX];
        char fullDestPath[PATH_MAX];

        struct dirent* entry;

        // Iterate through directory entries
        errno = 0;
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
                return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::BuildChildPath};
            }

            struct stat statbuf;
            if (::lstat(fullSourcePath, &statbuf) != 0)
            {
                return fileSystemResultFromNative(static_cast<uint32_t>(errno),
                                                  FileSystemErrorDetail::QueryChildMetadata);
            }

            // Recursively copy subdirectories and files
            SC_TRY(copyFile(StringSpan(fullSourcePath), StringSpan(fullDestPath), options, S_ISDIR(statbuf.st_mode)));
            errno = 0;
        }

        if (errno != 0)
            return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::EnumerateDirectory);

        return Result(true);
    }

    // Original file copying logic for non-directory files
    if (not options.overwrite)
    {
        struct stat destinationStat;
        if (::stat(destination.getNullTerminatedNative(), &destinationStat) == 0)
            return {FileSystemError::EntryAlreadyExists, FileSystemErrorDetail::CopyFileEntry};
        const int nativeError = errno;
        if (nativeError != ENOENT)
            return fileSystemResultFromNative(static_cast<uint32_t>(nativeError),
                                              FileSystemErrorDetail::QueryEntryMetadata);
    }
    int inputDescriptor = ::open(source.getNullTerminatedNative(), O_RDONLY);
    if (inputDescriptor < 0)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::OpenSourceFile);
    }
    auto closeInput = MakeDeferred([&] { ::close(inputDescriptor); });

    struct stat inputStat;

    if (::fstat(inputDescriptor, &inputStat) != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::QueryEntryMetadata);

    int outputDescriptor = ::open(destination.getNullTerminatedNative(), O_WRONLY | O_CREAT | O_TRUNC,
                                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (outputDescriptor < 0)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::OpenDestinationFile);
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
                return fileSystemResultFromNative(static_cast<uint32_t>(errno),
                                                  FileSystemErrorDetail::WriteFileContent);
            }
        }

        if (bytesRead < 0)
        {
            return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::ReadFileContent);
        }
    }
    return Result(true);
}

SC::ResultFileSystem SC::FileSystem::Operations::removeDirectoryRecursive(StringSpan path)
{
    SC_TRY(Internal::validatePath(path));

    // Open directory
    DIR* dir = ::opendir(path.getNullTerminatedNative());
    if (dir == nullptr)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::OpenSourceDirectory);
    }
    auto closeDir = MakeDeferred([&] { ::closedir(dir); });

    // Buffer for full path construction
    char fullPath[PATH_MAX];

    struct dirent* entry;

    // Iterate through directory entries
    errno = 0;
    while ((entry = ::readdir(dir)) != nullptr)
    {
        // Skip . and .. entries
        if (::strcmp(entry->d_name, ".") == 0 or ::strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full path
        if (::snprintf(fullPath, sizeof(fullPath), "%s/%s", path.getNullTerminatedNative(), entry->d_name) >=
            static_cast<int>(sizeof(fullPath)))
        {
            return {FileSystemError::PathCapacityExceeded, FileSystemErrorDetail::BuildChildPath};
        }

        struct stat statbuf;
        if (::lstat(fullPath, &statbuf) != 0)
        {
            return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::QueryChildMetadata);
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
                return fileSystemResultFromNative(static_cast<uint32_t>(errno),
                                                  FileSystemErrorDetail::RemoveChildEntry);
            }
        }
        errno = 0;
    }

    if (errno != 0)
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::EnumerateDirectory);

    // Remove the now-empty directory
    if (::rmdir(path.getNullTerminatedNative()) != 0)
    {
        return fileSystemResultFromNative(static_cast<uint32_t>(errno), FileSystemErrorDetail::RemoveDirectoryEntry);
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
