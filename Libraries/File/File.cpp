// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../File/File.h"
#include "../Common/Deferred.h"
#include "../Common/IGrowableBufferStringPath.h"
#include <memory.h> // memset

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

namespace SC
{
namespace FileWindowsDetail
{
#include "../Common/WindowsPath.inl"
}
} // namespace SC

namespace
{
static SC::ResultFile translateWindowsPathError(SC::FileWindowsDetail::WindowsPathResult result,
                                                SC::FileErrorDetail                      detail)
{
    using SC::FileError;
    using SC::FileWindowsDetail::WindowsPathError;
    switch (result.error)
    {
    case WindowsPathError::CapacityExceeded: return {FileError::PathCapacityExceeded, detail};
    case WindowsPathError::BasePathNotAbsolute: return {FileError::PathMustBeAbsolute, detail};
    case WindowsPathError::MalformedPath: return {FileError::InvalidPath, detail};
    case WindowsPathError::NativeCallFailed:
        return SC::ResultFile::withNativeError(FileError::InvalidPath, detail, result.nativeError);
    case WindowsPathError::None: return {};
    }
    return {FileError::InvalidPath, detail};
}

static SC::TimeMs fileDescriptorWindowsFileTimeToTimeMs(const FILETIME& fileTime)
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

static SC::FileDescriptorEntryType fileDescriptorWindowsEntryTypeFromAttributes(DWORD attributes, DWORD reparseTag)
{
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
    {
        if (reparseTag == IO_REPARSE_TAG_SYMLINK or reparseTag == 0)
        {
            return SC::FileDescriptorEntryType::SymbolicLink;
        }
        return SC::FileDescriptorEntryType::Other;
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        return SC::FileDescriptorEntryType::Directory;
    }
    return SC::FileDescriptorEntryType::File;
}

static SC::ResultFile fillFileDescriptorWindowsStat(HANDLE fileHandle, SC::FileDescriptorStat& fileStat)
{
    fileStat = {};

    BY_HANDLE_FILE_INFORMATION handleInfo;
    if (::GetFileInformationByHandle(fileHandle, &handleInfo) == FALSE)
        return SC::ResultFile::withNativeError(SC::FileError::MetadataQueryFailed,
                                               SC::FileErrorDetail::QueryDescriptorMetadata, ::GetLastError());

    FILE_STANDARD_INFO standardInfo = {};
    const bool         hasStandardInfo =
        ::GetFileInformationByHandleEx(fileHandle, FileStandardInfo, &standardInfo, sizeof(standardInfo)) != FALSE;

    FILE_ATTRIBUTE_TAG_INFO tagInfo = {};
    if (::GetFileInformationByHandleEx(fileHandle, FileAttributeTagInfo, &tagInfo, sizeof(tagInfo)) == FALSE)
    {
        tagInfo.FileAttributes = handleInfo.dwFileAttributes;
        tagInfo.ReparseTag     = 0;
    }

    fileStat.entryType = fileDescriptorWindowsEntryTypeFromAttributes(handleInfo.dwFileAttributes, tagInfo.ReparseTag);
    fileStat.creationTime = fileDescriptorWindowsFileTimeToTimeMs(handleInfo.ftCreationTime);
    fileStat.accessedTime = fileDescriptorWindowsFileTimeToTimeMs(handleInfo.ftLastAccessTime);
    fileStat.modifiedTime = fileDescriptorWindowsFileTimeToTimeMs(handleInfo.ftLastWriteTime);
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
} // namespace

//-------------------------------------------------------------------------------------------------------
// FileDescriptorDefinition
//-------------------------------------------------------------------------------------------------------
SC::ResultFile SC::detail::FileDescriptorDefinition::releaseHandle(Handle& handle)
{
    BOOL res;
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
    __try
    {
        res = ::CloseHandle(handle);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        res = FALSE;
    }
#else
    res = ::CloseHandle(handle);
#endif
    if (res == FALSE)
    {
        return ResultFile::withNativeError(FileError::CloseFailed, FileErrorDetail::CloseDescriptor, ::GetLastError());
    }
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// FileDescriptor
//-------------------------------------------------------------------------------------------------------
struct SC::FileDescriptor::Internal
{
    static ResultFile translateReadError(DWORD errorCode, FileErrorDetail detail)
    {
        switch (errorCode)
        {
        case ERROR_INVALID_HANDLE: return ResultFile::withNativeError(FileError::InvalidHandle, detail, errorCode);
        case ERROR_BROKEN_PIPE:
        case ERROR_NO_DATA: return ResultFile::withNativeError(FileError::PipeDisconnected, detail, errorCode);
        case ERROR_OPERATION_ABORTED:
            return ResultFile::withNativeError(FileError::OperationCancelled, detail, errorCode);
        case ERROR_IO_PENDING: return ResultFile::withNativeError(FileError::WouldBlock, detail, errorCode);
        }
        return ResultFile::withNativeError(FileError::ReadFailed, detail, errorCode);
    }

    static ResultFile readAppend(FileDescriptor::Handle fileDescriptor, IGrowableBuffer& buffer,
                                 Span<char> fallbackBuffer, bool& isEOF)
    {
        auto  bufferData   = buffer.getDirectAccess();
        DWORD numReadBytes = 0xffffffff;
        BOOL  success      = FALSE;
        DWORD lastError    = ERROR_SUCCESS;

        const bool useVector = bufferData.capacityInBytes > bufferData.sizeInBytes;
        if (useVector)
        {
            success = ::ReadFile(fileDescriptor, static_cast<char*>(bufferData.data) + bufferData.sizeInBytes,
                                 static_cast<DWORD>(bufferData.capacityInBytes - bufferData.sizeInBytes), &numReadBytes,
                                 nullptr);
            if (success == FALSE)
            {
                lastError = ::GetLastError();
            }
        }
        else
        {
            if (fallbackBuffer.sizeInBytes() == 0)
                return ResultFile(FileError::BufferCapacityExceeded, FileErrorDetail::ReadDescriptor);
            success = ::ReadFile(fileDescriptor, fallbackBuffer.data(),
                                 static_cast<DWORD>(fallbackBuffer.sizeInBytes()), &numReadBytes, nullptr);
            if (success == FALSE)
            {
                lastError = ::GetLastError();
            }
        }
        if (Internal::isActualError(success, numReadBytes, fileDescriptor, lastError))
        {
            return Internal::translateReadError(lastError, FileErrorDetail::ReadDescriptor);
        }
        else if (numReadBytes > 0)
        {
            if (not buffer.resizeWithoutInitializing(bufferData.sizeInBytes + static_cast<size_t>(numReadBytes)))
                return ResultFile(FileError::BufferCapacityExceeded, FileErrorDetail::GrowReadBuffer);
            if (not useVector)
            {
                auto newBufferData = buffer.getDirectAccess();
                ::memcpy(static_cast<char*>(newBufferData.data) + bufferData.sizeInBytes, fallbackBuffer.data(),
                         static_cast<size_t>(numReadBytes));
            }
            isEOF = false;
            return Result(true);
        }
        else
        {
            // EOF
            isEOF = true;
            return Result(true);
        }
    }

    [[nodiscard]] static bool isActualError(BOOL success, DWORD numReadBytes, FileDescriptor::Handle fileDescriptor,
                                            DWORD lastError)
    {
        if (success == FALSE and numReadBytes == 0 and GetFileType(fileDescriptor) == FILE_TYPE_PIPE and
            lastError == ERROR_BROKEN_PIPE)
        {
            // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
            // Pipes

            // If an anonymous pipe is being used and the write handle has been closed, when ReadFile attempts to read
            // using the pipe's corresponding read handle, the function returns FALSE and GetLastError returns
            // ERROR_BROKEN_PIPE.
            return false;
        }
        if (success == FALSE and numReadBytes == 0 and lastError == ERROR_HANDLE_EOF)
            return false;
        return success == FALSE;
    }
};

SC::ResultFile SC::FileDescriptor::seek(SeekMode seekMode, int64_t offset)
{
    if (not isValid())
        return {FileError::InvalidHandle, FileErrorDetail::SeekDescriptor};
    DWORD flags = 0;
    switch (seekMode)
    {
    case SeekMode::SeekStart: flags = FILE_BEGIN; break;
    case SeekMode::SeekEnd: flags = FILE_END; break;
    case SeekMode::SeekCurrent: flags = FILE_CURRENT; break;
    }
    LARGE_INTEGER distance;
    distance.QuadPart = offset;
    if (::SetFilePointerEx(handle, distance, nullptr, flags) == FALSE)
        return ResultFile::withNativeError(FileError::SeekFailed, FileErrorDetail::SeekDescriptor, ::GetLastError());
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::currentPosition(size_t& position) const
{
    if (not isValid())
        return {FileError::InvalidHandle, FileErrorDetail::QueryDescriptorPosition};
    LARGE_INTEGER li, source;
    memset(&source, 0, sizeof(source));
    if (::SetFilePointerEx(handle, source, &li, FILE_CURRENT) != 0)
    {
        position = static_cast<size_t>(li.QuadPart);
        return Result(true);
    }
    return ResultFile::withNativeError(FileError::SeekFailed, FileErrorDetail::QueryDescriptorPosition,
                                       ::GetLastError());
}

SC::ResultFile SC::FileDescriptor::sizeInBytes(size_t& sizeInBytes) const
{
    if (not isValid())
        return {FileError::InvalidHandle, FileErrorDetail::QueryDescriptorSize};
    LARGE_INTEGER li;
    if (GetFileSizeEx(handle, &li) != 0)
    {
        sizeInBytes = static_cast<size_t>(li.QuadPart);
        return Result(true);
    }
    return ResultFile::withNativeError(FileError::MetadataQueryFailed, FileErrorDetail::QueryDescriptorSize,
                                       ::GetLastError());
}

SC::ResultFile SC::FileDescriptor::stat(FileDescriptorStat& fileStat) const
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::QueryDescriptorMetadata);
    return fillFileDescriptorWindowsStat(handle, fileStat);
}

SC::ResultFile SC::FileDescriptor::chmod(uint32_t mode)
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::ChangeDescriptorPermissions);

    FILE_BASIC_INFO basicInfo = {};
    if (::GetFileInformationByHandleEx(handle, FileBasicInfo, &basicInfo, sizeof(basicInfo)) == FALSE)
        return ResultFile::withNativeError(FileError::MetadataQueryFailed, FileErrorDetail::ChangeDescriptorPermissions,
                                           ::GetLastError());

    if (mode & 0200u)
    {
        basicInfo.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
    }
    else
    {
        basicInfo.FileAttributes |= FILE_ATTRIBUTE_READONLY;
    }

    if (::SetFileInformationByHandle(handle, FileBasicInfo, &basicInfo, sizeof(basicInfo)) == FALSE)
        return ResultFile::withNativeError(FileError::PermissionsChangeFailed,
                                           FileErrorDetail::ChangeDescriptorPermissions, ::GetLastError());
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::chown(uint32_t uid, uint32_t gid)
{
    (void)uid;
    (void)gid;
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::ChangeDescriptorOwnership);
    return ResultFile(FileError::OperationUnsupported, FileErrorDetail::ChangeDescriptorOwnership);
}

SC::ResultFile SC::FileDescriptor::sync()
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::SynchronizeDescriptor);
    if (::FlushFileBuffers(handle) == FALSE)
        return ResultFile::withNativeError(FileError::SyncFailed, FileErrorDetail::SynchronizeDescriptor,
                                           ::GetLastError());
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::syncData()
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::SynchronizeDescriptorData);
    if (::FlushFileBuffers(handle) == FALSE)
        return ResultFile::withNativeError(FileError::SyncFailed, FileErrorDetail::SynchronizeDescriptorData,
                                           ::GetLastError());
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::truncate(uint64_t sizeInBytes)
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::TruncateDescriptor);

    LARGE_INTEGER currentPosition = {};
    LARGE_INTEGER newPosition;
    newPosition.QuadPart = 0;
    if (::SetFilePointerEx(handle, newPosition, &currentPosition, FILE_CURRENT) == FALSE)
        return ResultFile::withNativeError(FileError::TruncateFailed, FileErrorDetail::TruncateDescriptor,
                                           ::GetLastError());

    LARGE_INTEGER truncatePosition;
    truncatePosition.QuadPart = static_cast<LONGLONG>(sizeInBytes);
    if (::SetFilePointerEx(handle, truncatePosition, nullptr, FILE_BEGIN) == FALSE)
        return ResultFile::withNativeError(FileError::TruncateFailed, FileErrorDetail::TruncateDescriptor,
                                           ::GetLastError());
    if (::SetEndOfFile(handle) == FALSE)
        return ResultFile::withNativeError(FileError::TruncateFailed, FileErrorDetail::TruncateDescriptor,
                                           ::GetLastError());
    if (::SetFilePointerEx(handle, currentPosition, nullptr, FILE_BEGIN) == FALSE)
        return ResultFile::withNativeError(FileError::TruncateFailed, FileErrorDetail::TruncateDescriptor,
                                           ::GetLastError());
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::write(Span<const char> data, uint64_t offset)
{
    SC_TRY(seek(SeekStart, offset));
    ResultFile result = write(data);
    if (not result and result.detail == FileErrorDetail::WriteDescriptor)
        result.detail = FileErrorDetail::WriteDescriptorAtOffset;
    return result;
}

SC::ResultFile SC::FileDescriptor::write(Span<const char> data)
{
    DWORD      numberOfWrittenBytes;
    const BOOL res =
        ::WriteFile(handle, data.data(), static_cast<DWORD>(data.sizeInBytes()), &numberOfWrittenBytes, nullptr);
    if (res == FALSE)
    {
        const DWORD errorCode = ::GetLastError();
        if (errorCode == ERROR_INVALID_HANDLE)
            return ResultFile::withNativeError(FileError::InvalidHandle, FileErrorDetail::WriteDescriptor, errorCode);
        if (errorCode == ERROR_BROKEN_PIPE or errorCode == ERROR_NO_DATA)
            return ResultFile::withNativeError(FileError::PipeDisconnected, FileErrorDetail::WriteDescriptor,
                                               errorCode);
        if (errorCode == ERROR_OPERATION_ABORTED)
            return ResultFile::withNativeError(FileError::OperationCancelled, FileErrorDetail::WriteDescriptor,
                                               errorCode);
        if (errorCode == ERROR_IO_PENDING)
            return ResultFile::withNativeError(FileError::WouldBlock, FileErrorDetail::WriteDescriptor, errorCode);
        return ResultFile::withNativeError(FileError::WriteFailed, FileErrorDetail::WriteDescriptor, errorCode);
    }
    if (static_cast<size_t>(numberOfWrittenBytes) != data.sizeInBytes())
        return ResultFile::withActualBytes(FileError::IncompleteWrite, FileErrorDetail::WriteDescriptor,
                                           numberOfWrittenBytes);
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead, uint64_t offset)
{
    SC_TRY(seek(SeekStart, offset));
    ResultFile result = read(data, actuallyRead);
    if (not result and result.detail == FileErrorDetail::ReadDescriptor)
        result.detail = FileErrorDetail::ReadDescriptorAtOffset;
    return result;
}

SC::ResultFile SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead)
{
    DWORD      numberOfReadBytes = 0;
    const BOOL res =
        ::ReadFile(handle, data.data(), static_cast<DWORD>(data.sizeInBytes()), &numberOfReadBytes, nullptr);
    const DWORD lastError = res == FALSE ? ::GetLastError() : ERROR_SUCCESS;
    if (Internal::isActualError(res, numberOfReadBytes, handle, lastError))
    {
        return Internal::translateReadError(lastError, FileErrorDetail::ReadDescriptor);
    }
    if (not data.sliceStartLength(0, static_cast<size_t>(numberOfReadBytes), actuallyRead))
        return ResultFile(FileError::BufferCapacityExceeded, FileErrorDetail::ReadDescriptor);
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::open(StringSpan filePath, FileOpen mode)
{
    StringPath logicalPath;
    const auto logicalPathResult = FileWindowsDetail::WindowsPath::makeLogicalPath(filePath, logicalPath);
    if (not logicalPathResult)
        return translateWindowsPathError(logicalPathResult, FileErrorDetail::NormalizePath);
    if (logicalPath.view() != L"NUL" and not FileWindowsDetail::WindowsPath::isAbsolute(logicalPath.view()))
    {
        return ResultFile(FileError::PathMustBeAbsolute, FileErrorDetail::ValidateAbsolutePath);
    }
    const wchar_t* logicalData = logicalPath.view().getNullTerminatedNative();

    FileWindowsDetail::WindowsPath::TransportString transportPath;
    const wchar_t*                                  nullTerminatedPath;
    if (wcscmp(logicalData, L"NUL") == 0)
    {
        nullTerminatedPath = logicalData;
    }
    else
    {
        const auto transportPathResult =
            FileWindowsDetail::WindowsPath::appendTransportPrefix(logicalPath.view(), transportPath);
        if (not transportPathResult)
            return translateWindowsPathError(transportPathResult, FileErrorDetail::BuildTransportPath);
        nullTerminatedPath = transportPath.view().getNullTerminatedNative();
    }

    DWORD accessMode        = 0;
    DWORD createDisposition = 0;
    DWORD fileFlags         = mode.blocking ? 0 : FILE_FLAG_OVERLAPPED;

    switch (mode.mode)
    {
    case FileOpen::Read:
        accessMode |= FILE_GENERIC_READ;
        createDisposition = OPEN_EXISTING;
        break;
    case FileOpen::Write:
        accessMode |= FILE_GENERIC_WRITE;
        createDisposition = CREATE_ALWAYS;
        break;
    case FileOpen::Append:
        accessMode |= FILE_APPEND_DATA;
        createDisposition = OPEN_ALWAYS;
        break;
    case FileOpen::ReadWrite:
        accessMode |= FILE_GENERIC_READ | FILE_GENERIC_WRITE;
        createDisposition = OPEN_ALWAYS;
        break;
    case FileOpen::WriteRead:
        accessMode |= FILE_GENERIC_READ | FILE_GENERIC_WRITE;
        createDisposition = CREATE_ALWAYS;
        break;
    case FileOpen::AppendRead:
        accessMode |= FILE_GENERIC_READ | FILE_APPEND_DATA;
        createDisposition = OPEN_ALWAYS;
        break;
    }

    if (mode.sync)
    {
        fileFlags |= FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING;
    }

    if (mode.exclusive)
    {
        createDisposition = CREATE_NEW;
    }

    DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    SECURITY_ATTRIBUTES security;
    security.nLength              = sizeof(SECURITY_ATTRIBUTES);
    security.bInheritHandle       = mode.inheritable ? TRUE : FALSE;
    security.lpSecurityDescriptor = nullptr;

    HANDLE fileDescriptor =
        ::CreateFileW(nullTerminatedPath, accessMode, shareMode, &security, createDisposition, fileFlags, nullptr);

    if (fileDescriptor == INVALID_HANDLE_VALUE)
        return ResultFile::withNativeError(FileError::OpenFailed, FileErrorDetail::OpenFile, ::GetLastError());
    if (not assign(fileDescriptor))
    {
        ::CloseHandle(fileDescriptor);
        return {FileError::InvalidState, FileErrorDetail::OpenFile};
    }
    return Result(true);
}

#else
//! [UniqueHandleDefinitionSnippet]
#include <errno.h>      // errno
#include <fcntl.h>      // fcntl
#include <sys/socket.h> // socket/connect/accept
#include <sys/stat.h>   // fstat
#include <sys/un.h>     // sockaddr_un
#include <unistd.h>     // close

namespace
{
static SC::TimeMs fileDescriptorPosixTimespecToTimeMs(const timespec& timespecValue)
{
    return SC::TimeMs{static_cast<SC::int64_t>(timespecValue.tv_sec) * 1000 +
                      static_cast<SC::int64_t>(timespecValue.tv_nsec / (1000 * 1000))};
}

static SC::FileDescriptorEntryType fileDescriptorPosixEntryTypeFromMode(mode_t mode)
{
    if (S_ISREG(mode))
        return SC::FileDescriptorEntryType::File;
    if (S_ISDIR(mode))
        return SC::FileDescriptorEntryType::Directory;
    if (S_ISLNK(mode))
        return SC::FileDescriptorEntryType::SymbolicLink;
    return SC::FileDescriptorEntryType::Other;
}

static SC::ResultFile fillFileDescriptorPosixStat(const struct stat& pathStat, SC::FileDescriptorStat& fileStat)
{
    fileStat               = {};
    fileStat.entryType     = fileDescriptorPosixEntryTypeFromMode(pathStat.st_mode);
    fileStat.fileSize      = static_cast<SC::size_t>(pathStat.st_size);
    fileStat.hardLinkCount = static_cast<SC::size_t>(pathStat.st_nlink);
    fileStat.accessedTime  = fileDescriptorPosixTimespecToTimeMs(
#if __APPLE__
        pathStat.st_atimespec
#else
        pathStat.st_atim
#endif
    );
    fileStat.modifiedTime = fileDescriptorPosixTimespecToTimeMs(
#if __APPLE__
        pathStat.st_mtimespec
#else
        pathStat.st_mtim
#endif
    );
#if __APPLE__
    fileStat.creationTime = fileDescriptorPosixTimespecToTimeMs(pathStat.st_birthtimespec);
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
} // namespace

//-------------------------------------------------------------------------------------------------------
// FileDescriptorDefinition
//-------------------------------------------------------------------------------------------------------
SC::ResultFile SC::detail::FileDescriptorDefinition::releaseHandle(Handle& handle)
{
    if (::close(handle) != 0)
    {
        return ResultFile::withNativeError(FileError::CloseFailed, FileErrorDetail::CloseDescriptor,
                                           static_cast<uint32_t>(errno));
    }
    return Result(true);
}
//! [UniqueHandleDefinitionSnippet]

//-------------------------------------------------------------------------------------------------------
// FileDescriptor
//-------------------------------------------------------------------------------------------------------
struct SC::FileDescriptor::Internal
{
    static ResultFile translateReadError(int errorCode, FileErrorDetail detail)
    {
        const auto nativeError = static_cast<uint32_t>(errorCode);
        switch (errorCode)
        {
        case EAGAIN: return ResultFile::withNativeError(FileError::WouldBlock, detail, nativeError);
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK: return ResultFile::withNativeError(FileError::WouldBlock, detail, nativeError);
#endif
        case EBADF: return ResultFile::withNativeError(FileError::InvalidHandle, detail, nativeError);
#if defined(ECANCELED)
        case ECANCELED: return ResultFile::withNativeError(FileError::OperationCancelled, detail, nativeError);
#endif
#if defined(EPIPE)
        case EPIPE: return ResultFile::withNativeError(FileError::PipeDisconnected, detail, nativeError);
#endif
        }
        return ResultFile::withNativeError(FileError::ReadFailed, detail, nativeError);
    }

    static ResultFile translateWriteError(int errorCode, FileErrorDetail detail)
    {
        const auto nativeError = static_cast<uint32_t>(errorCode);
        switch (errorCode)
        {
        case EAGAIN: return ResultFile::withNativeError(FileError::WouldBlock, detail, nativeError);
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK: return ResultFile::withNativeError(FileError::WouldBlock, detail, nativeError);
#endif
        case EBADF: return ResultFile::withNativeError(FileError::InvalidHandle, detail, nativeError);
#if defined(ECANCELED)
        case ECANCELED: return ResultFile::withNativeError(FileError::OperationCancelled, detail, nativeError);
#endif
#if defined(EPIPE)
        case EPIPE: return ResultFile::withNativeError(FileError::PipeDisconnected, detail, nativeError);
#endif
        }
        return ResultFile::withNativeError(FileError::WriteFailed, detail, nativeError);
    }

    static ResultFile readAppend(FileDescriptor::Handle fileDescriptor, IGrowableBuffer& buffer,
                                 Span<char> fallbackBuffer, bool& isEOF)
    {
        auto       bufferData = buffer.getDirectAccess();
        ssize_t    numReadBytes;
        const bool useVector = bufferData.capacityInBytes > bufferData.sizeInBytes;
        if (useVector)
        {
            do
            {
                const size_t bytesToRead = (bufferData.capacityInBytes - bufferData.sizeInBytes);
                numReadBytes =
                    ::read(fileDescriptor, static_cast<char*>(bufferData.data) + bufferData.sizeInBytes, bytesToRead);
            } while (numReadBytes == -1 && errno == EINTR); // Syscall may be interrupted and userspace must retry
        }
        else
        {
            if (fallbackBuffer.sizeInBytes() == 0)
                return ResultFile(FileError::BufferCapacityExceeded, FileErrorDetail::ReadDescriptor);
            do
            {
                numReadBytes = ::read(fileDescriptor, fallbackBuffer.data(), fallbackBuffer.sizeInBytes());
            } while (numReadBytes == -1 && errno == EINTR); // Syscall may be interrupted and userspace must retry
        }
        if (numReadBytes > 0)
        {
            if (not buffer.resizeWithoutInitializing(bufferData.sizeInBytes + static_cast<size_t>(numReadBytes)))
                return ResultFile(FileError::BufferCapacityExceeded, FileErrorDetail::GrowReadBuffer);
            if (not useVector)
            {
                auto newBufferData = buffer.getDirectAccess();
                ::memcpy(static_cast<char*>(newBufferData.data) + bufferData.sizeInBytes, fallbackBuffer.data(),
                         static_cast<size_t>(numReadBytes));
            }
            isEOF = false;
            return Result(true);
        }
        else if (numReadBytes == 0)
        {
            // EOF
            isEOF = true;
            return Result(true);
        }
        else
        {
            return Internal::translateReadError(errno, FileErrorDetail::ReadDescriptor);
        }
    }

    static ResultFile setFileFlags(int flagRead, int flagWrite, const int fileDescriptor, const bool setFlag,
                                   const int flag)
    {
        int oldFlags;
        do
        {
            oldFlags = ::fcntl(fileDescriptor, flagRead);
        } while (oldFlags == -1 && errno == EINTR);
        if (oldFlags == -1)
            return ResultFile::withNativeError(FileError::DescriptorConfigurationFailed,
                                               FileErrorDetail::QueryDescriptorFlags, static_cast<uint32_t>(errno));
        const int newFlags = setFlag ? oldFlags | flag : oldFlags & (~flag);
        if (newFlags != oldFlags)
        {
            int res;
            do
            {
                res = ::fcntl(fileDescriptor, flagWrite, newFlags);
            } while (res == -1 && errno == EINTR);
            if (res != 0)
                return ResultFile::withNativeError(FileError::DescriptorConfigurationFailed,
                                                   FileErrorDetail::ConfigureDescriptorFlags,
                                                   static_cast<uint32_t>(errno));
        }
        return SC::Result(true);
    }

    template <int flag>
    static ResultFile setFileDescriptorFlags(int fileDescriptor, bool setFlag)
    {
        // We can OR the allowed flags here to provide some safety
        static_assert(flag == FD_CLOEXEC, "setFileStatusFlags invalid value");
        return setFileFlags(F_GETFD, F_SETFD, fileDescriptor, setFlag, flag);
    }

    template <int flag>
    static ResultFile setFileStatusFlags(int fileDescriptor, bool setFlag)
    {
        // We can OR the allowed flags here to provide some safety
        static_assert(flag == O_NONBLOCK, "setFileStatusFlags invalid value");
        return setFileFlags(F_GETFL, F_SETFL, fileDescriptor, setFlag, flag);
    }
};

int SC::FileOpen::toPosixFlags() const
{
    int flags = 0;
    switch (mode)
    {
    case FileOpen::Read: flags |= O_RDONLY; break;
    case FileOpen::Write: flags |= O_WRONLY | O_CREAT | O_TRUNC; break;
    case FileOpen::Append: flags |= O_WRONLY | O_APPEND | O_CREAT; break;
    case FileOpen::ReadWrite: flags |= O_RDWR; break;
    case FileOpen::WriteRead: flags |= O_RDWR | O_CREAT | O_TRUNC; break;
    case FileOpen::AppendRead: flags |= O_RDWR | O_APPEND | O_CREAT; break;
    }

    if (sync)
    {
        flags |= O_SYNC;
    }

    if (exclusive)
    {
        flags |= O_EXCL;
    }

    if (not inheritable)
    {
        flags |= O_CLOEXEC;
    }
    return flags;
}

int SC::FileOpen::toPosixAccess() const { return S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; }

SC::ResultFile SC::FileDescriptor::open(StringSpan filePath, FileOpen mode)
{
    if (filePath.getEncoding() == StringEncoding::Utf16)
        return ResultFile(FileError::UnsupportedPathEncoding, FileErrorDetail::ValidatePathEncoding);
    const int flags  = mode.toPosixFlags();
    const int access = mode.toPosixAccess();

    StringPath nullTerminated;
    if (not nullTerminated.assign(filePath))
        return ResultFile(FileError::PathCapacityExceeded, FileErrorDetail::NormalizePath);
    const char* nullTerminatedPath = nullTerminated.view().bytesIncludingTerminator();
    if (nullTerminatedPath[0] != '/')
        return ResultFile(FileError::PathMustBeAbsolute, FileErrorDetail::ValidateAbsolutePath);
    const int fileDescriptor = ::open(nullTerminatedPath, flags, access);
    if (fileDescriptor == -1)
        return ResultFile::withNativeError(FileError::OpenFailed, FileErrorDetail::OpenFile,
                                           static_cast<uint32_t>(errno));
    if (not assign(fileDescriptor))
    {
        ::close(fileDescriptor);
        return {FileError::InvalidState, FileErrorDetail::OpenFile};
    }
    if (not mode.blocking)
    {
        SC_TRY(Internal::setFileStatusFlags<O_NONBLOCK>(handle, true));
    }
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::seek(SeekMode seekMode, int64_t offset)
{
    if (not isValid())
        return {FileError::InvalidHandle, FileErrorDetail::SeekDescriptor};
    int flags = 0;
    switch (seekMode)
    {
    case SeekMode::SeekStart: flags = SEEK_SET; break;
    case SeekMode::SeekEnd: flags = SEEK_END; break;
    case SeekMode::SeekCurrent: flags = SEEK_CUR; break;
    }
    const off_t res = ::lseek(handle, static_cast<off_t>(offset), flags);
    if (res < 0)
        return ResultFile::withNativeError(FileError::SeekFailed, FileErrorDetail::SeekDescriptor,
                                           static_cast<uint32_t>(errno));
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::currentPosition(size_t& position) const
{
    if (not isValid())
        return {FileError::InvalidHandle, FileErrorDetail::QueryDescriptorPosition};
    const off_t fileSize = ::lseek(handle, 0, SEEK_CUR);
    if (fileSize < 0)
        return ResultFile::withNativeError(FileError::SeekFailed, FileErrorDetail::QueryDescriptorPosition,
                                           static_cast<uint32_t>(errno));
    position = static_cast<size_t>(fileSize);
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::sizeInBytes(size_t& sizeInBytes) const
{
    if (not isValid())
        return {FileError::InvalidHandle, FileErrorDetail::QueryDescriptorSize};
    struct stat fileStat;
    if (::fstat(handle, &fileStat) != 0)
        return ResultFile::withNativeError(FileError::MetadataQueryFailed, FileErrorDetail::QueryDescriptorSize,
                                           static_cast<uint32_t>(errno));
    sizeInBytes = static_cast<size_t>(fileStat.st_size);
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::stat(FileDescriptorStat& fileStat) const
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::QueryDescriptorMetadata);
    struct stat nativeStat;
    if (::fstat(handle, &nativeStat) != 0)
        return ResultFile::withNativeError(FileError::MetadataQueryFailed, FileErrorDetail::QueryDescriptorMetadata,
                                           static_cast<uint32_t>(errno));
    return fillFileDescriptorPosixStat(nativeStat, fileStat);
}

SC::ResultFile SC::FileDescriptor::chmod(uint32_t mode)
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::ChangeDescriptorPermissions);
    if (::fchmod(handle, static_cast<mode_t>(mode)) != 0)
        return ResultFile::withNativeError(FileError::PermissionsChangeFailed,
                                           FileErrorDetail::ChangeDescriptorPermissions, static_cast<uint32_t>(errno));
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::chown(uint32_t uid, uint32_t gid)
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::ChangeDescriptorOwnership);
    if (::fchown(handle, static_cast<uid_t>(uid), static_cast<gid_t>(gid)) != 0)
        return ResultFile::withNativeError(FileError::OwnershipChangeFailed, FileErrorDetail::ChangeDescriptorOwnership,
                                           static_cast<uint32_t>(errno));
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::sync()
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::SynchronizeDescriptor);
    if (::fsync(handle) != 0)
        return ResultFile::withNativeError(FileError::SyncFailed, FileErrorDetail::SynchronizeDescriptor,
                                           static_cast<uint32_t>(errno));
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::syncData()
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::SynchronizeDescriptorData);
#if __APPLE__
    if (::fsync(handle) != 0)
#else
    if (::fdatasync(handle) != 0)
#endif
        return ResultFile::withNativeError(FileError::SyncFailed, FileErrorDetail::SynchronizeDescriptorData,
                                           static_cast<uint32_t>(errno));
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::truncate(uint64_t sizeInBytes)
{
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::TruncateDescriptor);
    if (::ftruncate(handle, static_cast<off_t>(sizeInBytes)) != 0)
        return ResultFile::withNativeError(FileError::TruncateFailed, FileErrorDetail::TruncateDescriptor,
                                           static_cast<uint32_t>(errno));
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::write(Span<const char> data, uint64_t offset)
{
    ssize_t res;
    do
    {
        res = ::pwrite(handle, data.data(), data.sizeInBytes(), static_cast<off_t>(offset));
    } while (res == -1 and errno == EINTR);
    if (res < 0)
        return Internal::translateWriteError(errno, FileErrorDetail::WriteDescriptorAtOffset);
    if (static_cast<size_t>(res) != data.sizeInBytes())
    {
        if (static_cast<uint64_t>(res) <= 0xffffffffULL)
            return ResultFile::withActualBytes(FileError::IncompleteWrite, FileErrorDetail::WriteDescriptorAtOffset,
                                               static_cast<uint32_t>(res));
        return ResultFile(FileError::IncompleteWrite, FileErrorDetail::WriteDescriptorAtOffset);
    }
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::write(Span<const char> data)
{
    ssize_t res;
    do
    {
        res = ::write(handle, data.data(), data.sizeInBytes());
    } while (res == -1 and errno == EINTR);
    if (res < 0)
        return Internal::translateWriteError(errno, FileErrorDetail::WriteDescriptor);
    if (static_cast<size_t>(res) != data.sizeInBytes())
    {
        if (static_cast<uint64_t>(res) <= 0xffffffffULL)
            return ResultFile::withActualBytes(FileError::IncompleteWrite, FileErrorDetail::WriteDescriptor,
                                               static_cast<uint32_t>(res));
        return ResultFile(FileError::IncompleteWrite, FileErrorDetail::WriteDescriptor);
    }
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead, uint64_t offset)
{
    ssize_t res;
    do
    {
        res = ::pread(handle, data.data(), data.sizeInBytes(), static_cast<off_t>(offset));
    } while (res == -1 and errno == EINTR);
    if (res < 0)
        return Internal::translateReadError(errno, FileErrorDetail::ReadDescriptorAtOffset);
    if (not data.sliceStartLength(0, static_cast<size_t>(res), actuallyRead))
        return ResultFile(FileError::BufferCapacityExceeded, FileErrorDetail::ReadDescriptorAtOffset);
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::read(Span<char> data, Span<char>& actuallyRead)
{
    ssize_t res;
    do
    {
        res = ::read(handle, data.data(), data.sizeInBytes());
    } while (res == -1 and errno == EINTR);
    if (res < 0)
        return Internal::translateReadError(errno, FileErrorDetail::ReadDescriptor);
    if (not data.sliceStartLength(0, static_cast<size_t>(res), actuallyRead))
        return ResultFile(FileError::BufferCapacityExceeded, FileErrorDetail::ReadDescriptor);
    return Result(true);
}
#endif

//-------------------------------------------------------------------------------------------------------
// FileDescriptor (shared)
//-------------------------------------------------------------------------------------------------------

SC::ResultFile SC::FileDescriptor::openForWriteToDevNull()
{
#if SC_PLATFORM_WINDOWS
    return open(L"NUL", FileOpen::Append);
#else
    return open("/dev/null", FileOpen::Append);
#endif
}

SC::ResultFile SC::FileDescriptor::openStdOutDuplicate()
{
#if SC_PLATFORM_WINDOWS
    HANDLE stdHandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdHandle == INVALID_HANDLE_VALUE or stdHandle == nullptr)
    {
        return ResultFile::withNativeError(FileError::InvalidHandle, FileErrorDetail::GetStandardHandle,
                                           ::GetLastError());
    }
    HANDLE duplicated;
    BOOL   res = ::DuplicateHandle(::GetCurrentProcess(), stdHandle, ::GetCurrentProcess(), &duplicated, 0, TRUE,
                                   DUPLICATE_SAME_ACCESS);
    if (res == FALSE)
    {
        return ResultFile::withNativeError(FileError::DuplicateFailed, FileErrorDetail::DuplicateStandardHandle,
                                           ::GetLastError());
    }
    if (not assign(duplicated))
    {
        ::CloseHandle(duplicated);
        return {FileError::InvalidState, FileErrorDetail::DuplicateStandardHandle};
    }
    return Result(true);
#else
    const int duplicated = ::dup(STDOUT_FILENO);
    if (duplicated == -1)
        return ResultFile::withNativeError(FileError::DuplicateFailed, FileErrorDetail::DuplicateStandardHandle,
                                           static_cast<uint32_t>(errno));
    if (not assign(duplicated))
    {
        ::close(duplicated);
        return {FileError::InvalidState, FileErrorDetail::DuplicateStandardHandle};
    }
    return Result(true);
#endif
}

SC::ResultFile SC::FileDescriptor::openStdErrDuplicate()
{
#if SC_PLATFORM_WINDOWS
    HANDLE stdHandle = ::GetStdHandle(STD_ERROR_HANDLE);
    if (stdHandle == INVALID_HANDLE_VALUE or stdHandle == nullptr)
    {
        return ResultFile::withNativeError(FileError::InvalidHandle, FileErrorDetail::GetStandardHandle,
                                           ::GetLastError());
    }
    HANDLE duplicated;
    BOOL   res = ::DuplicateHandle(::GetCurrentProcess(), stdHandle, ::GetCurrentProcess(), &duplicated, 0, TRUE,
                                   DUPLICATE_SAME_ACCESS);
    if (res == FALSE)
    {
        return ResultFile::withNativeError(FileError::DuplicateFailed, FileErrorDetail::DuplicateStandardHandle,
                                           ::GetLastError());
    }
    if (not assign(duplicated))
    {
        ::CloseHandle(duplicated);
        return {FileError::InvalidState, FileErrorDetail::DuplicateStandardHandle};
    }
    return Result(true);
#else
    const int duplicated = ::dup(STDERR_FILENO);
    if (duplicated == -1)
        return ResultFile::withNativeError(FileError::DuplicateFailed, FileErrorDetail::DuplicateStandardHandle,
                                           static_cast<uint32_t>(errno));
    if (not assign(duplicated))
    {
        ::close(duplicated);
        return {FileError::InvalidState, FileErrorDetail::DuplicateStandardHandle};
    }
    return Result(true);
#endif
}

SC::ResultFile SC::FileDescriptor::openStdInDuplicate()
{
#if SC_PLATFORM_WINDOWS
    HANDLE stdHandle = ::GetStdHandle(STD_INPUT_HANDLE);
    if (stdHandle == INVALID_HANDLE_VALUE or stdHandle == nullptr)
    {
        return ResultFile::withNativeError(FileError::InvalidHandle, FileErrorDetail::GetStandardHandle,
                                           ::GetLastError());
    }
    HANDLE duplicated;
    BOOL   res = ::DuplicateHandle(::GetCurrentProcess(), stdHandle, ::GetCurrentProcess(), &duplicated, 0, TRUE,
                                   DUPLICATE_SAME_ACCESS);
    if (res == FALSE)
    {
        return ResultFile::withNativeError(FileError::DuplicateFailed, FileErrorDetail::DuplicateStandardHandle,
                                           ::GetLastError());
    }
    if (not assign(duplicated))
    {
        ::CloseHandle(duplicated);
        return {FileError::InvalidState, FileErrorDetail::DuplicateStandardHandle};
    }
    return Result(true);
#else
    const int duplicated = ::dup(STDIN_FILENO);
    if (duplicated == -1)
        return ResultFile::withNativeError(FileError::DuplicateFailed, FileErrorDetail::DuplicateStandardHandle,
                                           static_cast<uint32_t>(errno));
    if (not assign(duplicated))
    {
        ::close(duplicated);
        return {FileError::InvalidState, FileErrorDetail::DuplicateStandardHandle};
    }
    return Result(true);
#endif
}

SC::ResultFile SC::FileDescriptor::writeString(StringSpan data) { return write(data.toCharSpan()); }

SC::ResultFile SC::FileDescriptor::write(Span<const uint8_t> data, uint64_t offset)
{
    return write({reinterpret_cast<const char*>(data.data()), data.sizeInBytes()}, offset);
}

SC::ResultFile SC::FileDescriptor::read(Span<uint8_t> data, Span<uint8_t>& actuallyRead)
{
    Span<char> readBytes;
    SC_TRY(read({reinterpret_cast<char*>(data.data()), data.sizeInBytes()}, readBytes));
    actuallyRead = {reinterpret_cast<uint8_t*>(readBytes.data()), readBytes.sizeInBytes()};
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::read(Span<uint8_t> data, Span<uint8_t>& actuallyRead, uint64_t offset)
{
    Span<char> readBytes;
    SC_TRY(read({reinterpret_cast<char*>(data.data()), data.sizeInBytes()}, readBytes, offset));
    actuallyRead = {reinterpret_cast<uint8_t*>(readBytes.data()), readBytes.sizeInBytes()};
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::write(Span<const uint8_t> data)
{
    return write({reinterpret_cast<const char*>(data.data()), data.sizeInBytes()});
}

SC::ResultFile SC::FileDescriptor::readUntilFullOrEOF(Span<char> data, Span<char>& actuallyRead)
{
    auto availableData = data;
    while (not availableData.empty())
    {
        Span<char> readData;
        SC_TRY(read(availableData, readData));
        if (readData.empty())
            break;
        if (not availableData.sliceStartLength(readData.sizeInBytes(),
                                               availableData.sizeInBytes() - readData.sizeInBytes(), availableData))
            return {FileError::BufferCapacityExceeded, FileErrorDetail::ReadDescriptor};
    }
    if (not data.sliceStartLength(0, data.sizeInBytes() - availableData.sizeInBytes(), actuallyRead))
        return {FileError::BufferCapacityExceeded, FileErrorDetail::ReadDescriptor};
    return Result(true);
}

SC::ResultFile SC::FileDescriptor::readUntilEOF(IGrowableBuffer&& adapter)
{
    char buffer[1024];
    if (not isValid())
        return ResultFile(FileError::InvalidHandle, FileErrorDetail::ReadDescriptor);
    bool isEOF = false;
    if (not adapter.resizeWithoutInitializing(0))
        return ResultFile(FileError::BufferCapacityExceeded, FileErrorDetail::ResetReadBuffer);
    while (not isEOF)
    {
        SC_TRY(Internal::readAppend(handle, adapter, {buffer, sizeof(buffer)}, isEOF));
    }
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// PipeDescriptor
//-------------------------------------------------------------------------------------------------------
#if SC_PLATFORM_WINDOWS
#include <stdio.h>
SC::ResultFile SC::PipeDescriptor::createPipe(PipeOptions options)
{
    // On Windows to inherit flags they must be flagged as inheritable
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
    SECURITY_ATTRIBUTES security;
    ::memset(&security, 0, sizeof(security));
    security.nLength              = sizeof(security);
    security.bInheritHandle       = options.readInheritable or options.writeInheritable ? TRUE : FALSE;
    security.lpSecurityDescriptor = nullptr;

    HANDLE pipeRead  = INVALID_HANDLE_VALUE;
    HANDLE pipeWrite = INVALID_HANDLE_VALUE;

    if (options.blocking == false)
    {
        char pipeName[64];
#if SC_PLATFORM_64_BIT
        snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\SC-%lu-%llu", ::GetCurrentProcessId(), (intptr_t)this);
#else
        snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\SC-%lu-%lu", ::GetCurrentProcessId(), (intptr_t)this);
#endif

        DWORD pipeFlags = PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED;
        DWORD pipeMode  = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;

        pipeRead = ::CreateNamedPipeA(pipeName, pipeFlags, pipeMode, 1, 65536, 65536, 0, &security);
        if (pipeRead == INVALID_HANDLE_VALUE)
        {
            return ResultFile::withNativeError(FileError::PipeCreationFailed, FileErrorDetail::CreateAnonymousPipe,
                                               ::GetLastError());
        }
        pipeWrite = ::CreateFileA(pipeName, GENERIC_WRITE | FILE_READ_ATTRIBUTES, 0, &security, OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED, nullptr);
        if (pipeWrite == INVALID_HANDLE_VALUE)
        {
            const DWORD errorCode = ::GetLastError();
            ::CloseHandle(pipeRead);
            return ResultFile::withNativeError(FileError::PipeCreationFailed, FileErrorDetail::CreateAnonymousPipe,
                                               errorCode);
        }
        if (::ConnectNamedPipe(pipeRead, nullptr) == FALSE) // Connect the pipe immediately
        {
            if (GetLastError() != ERROR_PIPE_CONNECTED)
            {
                const DWORD errorCode = ::GetLastError();
                ::CloseHandle(pipeRead);
                ::CloseHandle(pipeWrite);
                return ResultFile::withNativeError(FileError::PipeCreationFailed, FileErrorDetail::CreateAnonymousPipe,
                                                   errorCode);
            }
        }
    }
    else
    {
        if (::CreatePipe(&pipeRead, &pipeWrite, &security, 0) == FALSE)
        {
            return ResultFile::withNativeError(FileError::PipeCreationFailed, FileErrorDetail::CreateAnonymousPipe,
                                               ::GetLastError());
        }
    }
    if (not readPipe.assign(pipeRead))
    {
        ::CloseHandle(pipeRead);
        ::CloseHandle(pipeWrite);
        return {FileError::InvalidState, FileErrorDetail::AssignPipeDescriptor};
    }
    if (not writePipe.assign(pipeWrite))
    {
        ::CloseHandle(pipeWrite);
        (void)readPipe.close();
        return {FileError::InvalidState, FileErrorDetail::AssignPipeDescriptor};
    }

    if (security.bInheritHandle)
    {
        if (not options.readInheritable)
        {
            if (::SetHandleInformation(pipeRead, HANDLE_FLAG_INHERIT, FALSE) == FALSE)
            {
                return ResultFile::withNativeError(FileError::DescriptorConfigurationFailed,
                                                   FileErrorDetail::ConfigureDescriptorFlags, ::GetLastError());
            }
        }
        if (not options.writeInheritable)
        {
            if (::SetHandleInformation(pipeWrite, HANDLE_FLAG_INHERIT, FALSE) == FALSE)
            {
                return ResultFile::withNativeError(FileError::DescriptorConfigurationFailed,
                                                   FileErrorDetail::ConfigureDescriptorFlags, ::GetLastError());
            }
        }
    }
    return Result(true);
}

#else
namespace
{
static SC::ResultFile movePosixDescriptorAboveStandardRange(int& descriptor)
{
    if (descriptor >= 3)
    {
        return SC::Result(true);
    }

#if defined(F_DUPFD_CLOEXEC)
    int movedDescriptor;
    do
    {
        movedDescriptor = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 3);
    } while (movedDescriptor == -1 and errno == EINTR);
#else
    int movedDescriptor;
    do
    {
        movedDescriptor = ::fcntl(descriptor, F_DUPFD, 3);
    } while (movedDescriptor == -1 and errno == EINTR);
#endif
    if (movedDescriptor == -1)
        return SC::ResultFile::withNativeError(SC::FileError::DuplicateFailed,
                                               SC::FileErrorDetail::DuplicatePipeDescriptor,
                                               static_cast<SC::uint32_t>(errno));
    ::close(descriptor);
    descriptor = movedDescriptor;
    return SC::Result(true);
}
} // namespace

SC::ResultFile SC::PipeDescriptor::createPipe(PipeOptions options)
{
    int  pipes[2];
    int  res                     = -1;
    bool usedPipeCreationFlags   = false;
    bool usedNonBlockingAtCreate = false;
#if SC_PLATFORM_LINUX
    int pipeFlags = O_CLOEXEC;
    if (options.blocking == false)
    {
        pipeFlags |= O_NONBLOCK;
    }
    do
    {
        res = ::pipe2(pipes, pipeFlags);
    } while (res == -1 and errno == EINTR);
    if (res == 0)
    {
        usedPipeCreationFlags   = true;
        usedNonBlockingAtCreate = options.blocking == false;
    }
    else if (errno != ENOSYS and errno != EINVAL)
    {
        return ResultFile::withNativeError(FileError::PipeCreationFailed, FileErrorDetail::CreateAnonymousPipe,
                                           static_cast<uint32_t>(errno));
    }
#endif
    if (not usedPipeCreationFlags)
    {
        do
        {
            res = ::pipe(pipes);
        } while (res == -1 and errno == EINTR);
    }

    if (res != 0)
        return ResultFile::withNativeError(FileError::PipeCreationFailed, FileErrorDetail::CreateAnonymousPipe,
                                           static_cast<uint32_t>(errno));
    const auto closePipes = MakeDeferred(
        [&]
        {
            if (pipes[0] >= 0)
            {
                ::close(pipes[0]);
            }
            if (pipes[1] >= 0)
            {
                ::close(pipes[1]);
            }
        });
    SC_TRY(movePosixDescriptorAboveStandardRange(pipes[0]));
    SC_TRY(movePosixDescriptorAboveStandardRange(pipes[1]));
    const int readDescriptor  = pipes[0];
    const int writeDescriptor = pipes[1];
    if (not readPipe.assign(pipes[0]))
        return {FileError::InvalidState, FileErrorDetail::AssignPipeDescriptor};
    pipes[0] = -1;
    if (not writePipe.assign(pipes[1]))
        return {FileError::InvalidState, FileErrorDetail::AssignPipeDescriptor};
    pipes[1] = -1;
    const ResultFile setReadCloExec =
        FileDescriptor::Internal::setFileDescriptorFlags<FD_CLOEXEC>(readDescriptor, not options.readInheritable);
    SC_TRY(setReadCloExec);
    const ResultFile setWriteCloExec =
        FileDescriptor::Internal::setFileDescriptorFlags<FD_CLOEXEC>(writeDescriptor, not options.writeInheritable);
    SC_TRY(setWriteCloExec);
    if (options.blocking == false and not usedNonBlockingAtCreate)
    {
        const ResultFile pipeRes1 = FileDescriptor::Internal::setFileStatusFlags<O_NONBLOCK>(readDescriptor, true);
        SC_TRY(pipeRes1);
        const ResultFile pipeRes2 = FileDescriptor::Internal::setFileStatusFlags<O_NONBLOCK>(writeDescriptor, true);
        SC_TRY(pipeRes2);
    }
    return Result(true);
}
#endif

SC::ResultFile SC::PipeDescriptor::close()
{
    const ResultFile readResult  = readPipe.close();
    const ResultFile writeResult = writePipe.close();
    return readResult ? writeResult : readResult;
}

namespace
{
static SC::ResultFile validateNamedPipeLogicalName(SC::StringSpan logicalName)
{
    if (logicalName.getEncoding() == SC::StringEncoding::Utf16)
        return {SC::FileError::UnsupportedPathEncoding, SC::FileErrorDetail::ValidateNamedPipeLogicalName};
    if (logicalName.isEmpty())
        return {SC::FileError::InvalidNamedPipeName, SC::FileErrorDetail::ValidateNamedPipeLogicalName};

    const char*  bytes = logicalName.bytesWithoutTerminator();
    const size_t size  = logicalName.sizeInBytes();
    for (size_t i = 0; i < size; ++i)
    {
        if (bytes[i] == '\0' or bytes[i] == '/' or bytes[i] == '\\')
            return {SC::FileError::InvalidNamedPipeName, SC::FileErrorDetail::ValidateNamedPipeLogicalName};
    }
    return SC::Result(true);
}
} // namespace

SC::ResultFile SC::NamedPipeName::build(StringSpan logicalName, StringPath& outName, NamedPipeNameOptions options)
{
    SC_TRY(validateNamedPipeLogicalName(logicalName));

    StringPath nativeName;
#if SC_PLATFORM_WINDOWS
    (void)options;
    if (not nativeName.assign("\\\\.\\pipe\\") or not nativeName.append(logicalName))
        return {FileError::PathCapacityExceeded, FileErrorDetail::BuildNamedPipePath};
#else
    if (options.posixDirectory.getEncoding() == StringEncoding::Utf16)
        return {FileError::UnsupportedPathEncoding, FileErrorDetail::BuildNamedPipePath};
    if (options.posixDirectory.isEmpty())
        return {FileError::InvalidPath, FileErrorDetail::BuildNamedPipePath};
    const char* posixDirectoryBytes = options.posixDirectory.bytesWithoutTerminator();
    if (posixDirectoryBytes[0] != '/')
        return {FileError::PathMustBeAbsolute, FileErrorDetail::ValidateAbsolutePath};

    if (not nativeName.assign(options.posixDirectory))
        return {FileError::PathCapacityExceeded, FileErrorDetail::BuildNamedPipePath};
    if (posixDirectoryBytes[options.posixDirectory.sizeInBytes() - 1] != '/')
    {
        if (not nativeName.append("/"))
            return {FileError::PathCapacityExceeded, FileErrorDetail::BuildNamedPipePath};
    }
    if (not nativeName.append(logicalName))
        return {FileError::PathCapacityExceeded, FileErrorDetail::BuildNamedPipePath};
#endif
    outName = move(nativeName);
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// NamedPipe
//-------------------------------------------------------------------------------------------------------

#if SC_PLATFORM_WINDOWS
namespace
{
using SC::FileDescriptor;
using SC::NamedPipeServerOptions;
using SC::PipeDescriptor;
using SC::PipeOptions;
using SC::Result;
using SC::ResultFile;
using SC::StringSpan;

static bool hasWindowsNamedPipePrefix(const wchar_t* fullName)
{
    constexpr auto dotPrefix      = L"\\\\.\\pipe\\";
    constexpr auto questionPrefix = L"\\\\?\\pipe\\";
    return ::wcsncmp(fullName, dotPrefix, 9) == 0 or ::wcsncmp(fullName, questionPrefix, 9) == 0;
}

static ResultFile duplicateConnectedPipeHandle(HANDLE connectedHandle, PipeOptions options,
                                               PipeDescriptor& outConnection)
{
    HANDLE readHandle  = INVALID_HANDLE_VALUE;
    HANDLE writeHandle = INVALID_HANDLE_VALUE;
    if (::DuplicateHandle(::GetCurrentProcess(), connectedHandle, ::GetCurrentProcess(), &readHandle, 0,
                          options.readInheritable ? TRUE : FALSE, DUPLICATE_SAME_ACCESS) == FALSE)
    {
        return ResultFile::withNativeError(SC::FileError::DuplicateFailed,
                                           SC::FileErrorDetail::DuplicateNamedPipeConnection, ::GetLastError());
    }
    if (::DuplicateHandle(::GetCurrentProcess(), connectedHandle, ::GetCurrentProcess(), &writeHandle, 0,
                          options.writeInheritable ? TRUE : FALSE, DUPLICATE_SAME_ACCESS) == FALSE)
    {
        const DWORD errorCode = ::GetLastError();
        ::CloseHandle(readHandle);
        return ResultFile::withNativeError(SC::FileError::DuplicateFailed,
                                           SC::FileErrorDetail::DuplicateNamedPipeConnection, errorCode);
    }
    PipeDescriptor duplicated;
    if (not duplicated.readPipe.assign(readHandle))
    {
        ::CloseHandle(readHandle);
        ::CloseHandle(writeHandle);
        return {SC::FileError::InvalidState, SC::FileErrorDetail::AssignPipeDescriptor};
    }
    if (not duplicated.writePipe.assign(writeHandle))
    {
        ::CloseHandle(writeHandle);
        return {SC::FileError::InvalidState, SC::FileErrorDetail::AssignPipeDescriptor};
    }
    outConnection = move(duplicated);
    return Result(true);
}

static ResultFile createPendingServerInstance(StringSpan pipeName, const NamedPipeServerOptions& options,
                                              bool firstInstance, FileDescriptor& pendingConnection)
{
    const wchar_t* nullTerminatedName = pipeName.getNullTerminatedNative();

    DWORD openMode = PIPE_ACCESS_DUPLEX;
    if (not options.connectionOptions.blocking)
    {
        openMode |= FILE_FLAG_OVERLAPPED;
    }
    if (firstInstance)
    {
        openMode |= FILE_FLAG_FIRST_PIPE_INSTANCE;
    }

    DWORD maxPendingConnections = options.maxPendingConnections;
    if (maxPendingConnections == 0)
    {
        maxPendingConnections = 1;
    }
    if (maxPendingConnections > PIPE_UNLIMITED_INSTANCES)
    {
        maxPendingConnections = PIPE_UNLIMITED_INSTANCES;
    }

    const DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;

    HANDLE handle =
        ::CreateNamedPipeW(nullTerminatedName, openMode, pipeMode, maxPendingConnections, 65536, 65536, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return ResultFile::withNativeError(SC::FileError::NamedPipeCreationFailed,
                                           SC::FileErrorDetail::CreateNamedPipeServer, ::GetLastError());

    if (not pendingConnection.assign(handle))
    {
        ::CloseHandle(handle);
        return {SC::FileError::InvalidState, SC::FileErrorDetail::CreateNamedPipeServer};
    }
    return Result(true);
}
} // namespace

SC::ResultFile SC::NamedPipeServer::create(StringSpan pipeName, NamedPipeServerOptions pipeOptions)
{
    if (created)
        return {FileError::InvalidState, FileErrorDetail::CreateNamedPipeServer};
    if (not name.assign(pipeName))
        return {FileError::PathCapacityExceeded, FileErrorDetail::BuildNamedPipePath};

    const wchar_t* fullName = name.view().getNullTerminatedNative();
    if (not hasWindowsNamedPipePrefix(fullName))
        return {FileError::InvalidNamedPipeName, FileErrorDetail::ValidateNamedPipePath};

    options       = pipeOptions;
    firstInstance = true;
    SC_TRY(createPendingServerInstance(name.view(), options, firstInstance, pendingConnection));
    firstInstance = false;
    created       = true;
    return Result(true);
}

SC::ResultFile SC::NamedPipeServer::accept(PipeDescriptor& outConnection)
{
    if (not created or not pendingConnection.isValid())
        return {FileError::InvalidState, FileErrorDetail::AcceptNamedPipeConnection};
    HANDLE pendingHandle;
    if (not pendingConnection.get(pendingHandle, false))
        return {FileError::InvalidHandle, FileErrorDetail::AcceptNamedPipeConnection};

    if (::ConnectNamedPipe(pendingHandle, nullptr) == FALSE)
    {
        const DWORD errorCode = ::GetLastError();
        if (errorCode != ERROR_PIPE_CONNECTED)
            return ResultFile::withNativeError(FileError::NamedPipeAcceptFailed,
                                               FileErrorDetail::AcceptNamedPipeConnection, errorCode);
    }

    PipeDescriptor connected;
    SC_TRY(duplicateConnectedPipeHandle(pendingHandle, options.connectionOptions, connected));

    SC_TRY(pendingConnection.close());
    SC_TRY(createPendingServerInstance(name.view(), options, false, pendingConnection));

    outConnection = move(connected);
    return Result(true);
}

SC::ResultFile SC::NamedPipeServer::close()
{
    if (not created)
    {
        return Result(true);
    }
    created       = false;
    firstInstance = true;
    return pendingConnection.close();
}

SC::ResultFile SC::NamedPipeClient::connect(StringSpan pipeName, PipeDescriptor& outConnection,
                                            NamedPipeClientOptions options)
{
    StringPath nullTerminatedPath;
    if (not nullTerminatedPath.assign(pipeName))
        return {FileError::PathCapacityExceeded, FileErrorDetail::BuildNamedPipePath};

    const wchar_t* fullName = nullTerminatedPath.view().getNullTerminatedNative();
    if (not hasWindowsNamedPipePrefix(fullName))
        return {FileError::InvalidNamedPipeName, FileErrorDetail::ValidateNamedPipePath};

    const DWORD fileFlags = options.connectionOptions.blocking ? 0 : FILE_FLAG_OVERLAPPED;

    HANDLE clientHandle =
        ::CreateFileW(fullName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, fileFlags, nullptr);
    if (clientHandle == INVALID_HANDLE_VALUE and ::GetLastError() == ERROR_PIPE_BUSY)
    {
        if (::WaitNamedPipeW(fullName, options.windows.connectTimeoutMilliseconds) == FALSE)
        {
            const DWORD errorCode = ::GetLastError();
            if (errorCode == ERROR_SEM_TIMEOUT)
                return ResultFile::withNativeError(FileError::TimedOut, FileErrorDetail::WaitNamedPipeClient,
                                                   errorCode);
            return ResultFile::withNativeError(FileError::NamedPipeConnectionFailed,
                                               FileErrorDetail::WaitNamedPipeClient, errorCode);
        }
        clientHandle =
            ::CreateFileW(fullName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, fileFlags, nullptr);
    }
    if (clientHandle == INVALID_HANDLE_VALUE)
        return ResultFile::withNativeError(FileError::NamedPipeConnectionFailed,
                                           FileErrorDetail::ConnectNamedPipeClient, ::GetLastError());

    FileDescriptor connected;
    if (not connected.assign(clientHandle))
    {
        ::CloseHandle(clientHandle);
        return {FileError::InvalidState, FileErrorDetail::ConnectNamedPipeClient};
    }

    HANDLE connectedHandle;
    if (not connected.get(connectedHandle, false))
        return {FileError::InvalidHandle, FileErrorDetail::ConnectNamedPipeClient};
    PipeDescriptor duplicated;
    SC_TRY(duplicateConnectedPipeHandle(connectedHandle, options.connectionOptions, duplicated));
    outConnection = move(duplicated);
    return Result(true);
}

#else

namespace
{
using SC::PipeDescriptor;
using SC::PipeOptions;
using SC::Result;
using SC::ResultFile;

static ResultFile setPosixDescriptorInheritable(int descriptor, bool inheritable)
{
    int flags;
    do
    {
        flags = ::fcntl(descriptor, F_GETFD);
    } while (flags == -1 and errno == EINTR);
    if (flags == -1)
        return ResultFile::withNativeError(SC::FileError::DescriptorConfigurationFailed,
                                           SC::FileErrorDetail::QueryDescriptorFlags, static_cast<SC::uint32_t>(errno));

    const int wantedFlags = inheritable ? (flags & (~FD_CLOEXEC)) : (flags | FD_CLOEXEC);
    if (wantedFlags != flags)
    {
        int res;
        do
        {
            res = ::fcntl(descriptor, F_SETFD, wantedFlags);
        } while (res == -1 and errno == EINTR);
        if (res != 0)
            return ResultFile::withNativeError(SC::FileError::DescriptorConfigurationFailed,
                                               SC::FileErrorDetail::ConfigureDescriptorFlags,
                                               static_cast<SC::uint32_t>(errno));
    }
    return Result(true);
}

static ResultFile setPosixDescriptorBlocking(int descriptor, bool blocking)
{
    int flags;
    do
    {
        flags = ::fcntl(descriptor, F_GETFL);
    } while (flags == -1 and errno == EINTR);
    if (flags == -1)
        return ResultFile::withNativeError(SC::FileError::DescriptorConfigurationFailed,
                                           SC::FileErrorDetail::QueryDescriptorFlags, static_cast<SC::uint32_t>(errno));

    const int wantedFlags = blocking ? (flags & (~O_NONBLOCK)) : (flags | O_NONBLOCK);
    if (wantedFlags != flags)
    {
        int res;
        do
        {
            res = ::fcntl(descriptor, F_SETFL, wantedFlags);
        } while (res == -1 and errno == EINTR);
        if (res != 0)
            return ResultFile::withNativeError(SC::FileError::DescriptorConfigurationFailed,
                                               SC::FileErrorDetail::ConfigureDescriptorFlags,
                                               static_cast<SC::uint32_t>(errno));
    }
    return Result(true);
}

static ResultFile duplicateConnectedSocket(int connectedDescriptor, PipeOptions options, PipeDescriptor& outConnection)
{
    int readDescriptor;
    do
    {
        readDescriptor = ::dup(connectedDescriptor);
    } while (readDescriptor == -1 and errno == EINTR);
    if (readDescriptor == -1)
        return ResultFile::withNativeError(SC::FileError::DuplicateFailed,
                                           SC::FileErrorDetail::DuplicateNamedPipeConnection,
                                           static_cast<SC::uint32_t>(errno));

    int writeDescriptor;
    do
    {
        writeDescriptor = ::dup(connectedDescriptor);
    } while (writeDescriptor == -1 and errno == EINTR);
    if (writeDescriptor == -1)
    {
        const int errorCode = errno;
        ::close(readDescriptor);
        return ResultFile::withNativeError(SC::FileError::DuplicateFailed,
                                           SC::FileErrorDetail::DuplicateNamedPipeConnection,
                                           static_cast<SC::uint32_t>(errorCode));
    }

    PipeDescriptor duplicated;
    if (not duplicated.readPipe.assign(readDescriptor))
    {
        ::close(readDescriptor);
        ::close(writeDescriptor);
        return {SC::FileError::InvalidState, SC::FileErrorDetail::AssignPipeDescriptor};
    }
    if (not duplicated.writePipe.assign(writeDescriptor))
    {
        ::close(writeDescriptor);
        return {SC::FileError::InvalidState, SC::FileErrorDetail::AssignPipeDescriptor};
    }

    int rawDescriptor = -1;
    if (not duplicated.readPipe.get(rawDescriptor, false))
        return {SC::FileError::InvalidHandle, SC::FileErrorDetail::DuplicateNamedPipeConnection};
    SC_TRY(setPosixDescriptorInheritable(rawDescriptor, options.readInheritable));
    SC_TRY(setPosixDescriptorBlocking(rawDescriptor, options.blocking));

    if (not duplicated.writePipe.get(rawDescriptor, false))
        return {SC::FileError::InvalidHandle, SC::FileErrorDetail::DuplicateNamedPipeConnection};
    SC_TRY(setPosixDescriptorInheritable(rawDescriptor, options.writeInheritable));
    SC_TRY(setPosixDescriptorBlocking(rawDescriptor, options.blocking));

    outConnection = move(duplicated);
    return Result(true);
}
} // namespace

SC::ResultFile SC::NamedPipeServer::create(StringSpan pipeName, NamedPipeServerOptions pipeOptions)
{
    if (created)
        return {FileError::InvalidState, FileErrorDetail::CreateNamedPipeServer};
    if (pipeName.getEncoding() == StringEncoding::Utf16)
        return {FileError::UnsupportedPathEncoding, FileErrorDetail::ValidateNamedPipePath};
    if (not name.assign(pipeName))
        return {FileError::PathCapacityExceeded, FileErrorDetail::BuildNamedPipePath};

    const char* fullPath = name.view().bytesIncludingTerminator();
    if (fullPath[0] != '/')
        return {FileError::PathMustBeAbsolute, FileErrorDetail::ValidateAbsolutePath};
    sockaddr_un sizeCheck;
    if (::strlen(fullPath) >= sizeof(sizeCheck.sun_path))
        return {FileError::PathCapacityExceeded, FileErrorDetail::ValidateNamedPipePath};

    options = pipeOptions;
    if (options.posix.removeEndpointBeforeCreate)
    {
        int res;
        do
        {
            res = ::unlink(fullPath);
        } while (res == -1 and errno == EINTR);
        if (res != 0 and errno != ENOENT)
            return ResultFile::withNativeError(FileError::EndpointCleanupFailed,
                                               FileErrorDetail::RemoveNamedPipeEndpoint, static_cast<uint32_t>(errno));
    }

    int listeningDescriptor;
    do
    {
        listeningDescriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
    } while (listeningDescriptor == -1 and errno == EINTR);
    if (listeningDescriptor == -1)
        return ResultFile::withNativeError(FileError::NamedPipeCreationFailed, FileErrorDetail::CreateNamedPipeServer,
                                           static_cast<uint32_t>(errno));

    if (not listeningSocket.assign(listeningDescriptor))
    {
        ::close(listeningDescriptor);
        return {FileError::InvalidState, FileErrorDetail::CreateNamedPipeServer};
    }

    sockaddr_un address;
    ::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    ::memcpy(address.sun_path, fullPath, ::strlen(fullPath) + 1);

    int bindResult;
    do
    {
        bindResult = ::bind(listeningDescriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    } while (bindResult == -1 and errno == EINTR);
    if (bindResult != 0)
        return ResultFile::withNativeError(FileError::NamedPipeCreationFailed, FileErrorDetail::BindNamedPipeServer,
                                           static_cast<uint32_t>(errno));

    int backlog = static_cast<int>(options.maxPendingConnections);
    if (backlog <= 0)
    {
        backlog = 1;
    }
    int listenResult;
    do
    {
        listenResult = ::listen(listeningDescriptor, backlog);
    } while (listenResult == -1 and errno == EINTR);
    if (listenResult != 0)
        return ResultFile::withNativeError(FileError::NamedPipeCreationFailed, FileErrorDetail::ListenNamedPipeServer,
                                           static_cast<uint32_t>(errno));

    created = true;
    return Result(true);
}

SC::ResultFile SC::NamedPipeServer::accept(PipeDescriptor& outConnection)
{
    if (not created or not listeningSocket.isValid())
        return {FileError::InvalidState, FileErrorDetail::AcceptNamedPipeConnection};
    int listeningDescriptor;
    if (not listeningSocket.get(listeningDescriptor, false))
        return {FileError::InvalidHandle, FileErrorDetail::AcceptNamedPipeConnection};

    int acceptedDescriptor;
    do
    {
        acceptedDescriptor = ::accept(listeningDescriptor, nullptr, nullptr);
    } while (acceptedDescriptor == -1 and errno == EINTR);
    if (acceptedDescriptor == -1)
        return ResultFile::withNativeError(FileError::NamedPipeAcceptFailed, FileErrorDetail::AcceptNamedPipeConnection,
                                           static_cast<uint32_t>(errno));

    FileDescriptor acceptedSocket;
    if (not acceptedSocket.assign(acceptedDescriptor))
    {
        ::close(acceptedDescriptor);
        return {FileError::InvalidState, FileErrorDetail::AcceptNamedPipeConnection};
    }

    int rawAcceptedDescriptor;
    if (not acceptedSocket.get(rawAcceptedDescriptor, false))
        return {FileError::InvalidHandle, FileErrorDetail::AcceptNamedPipeConnection};
    PipeDescriptor duplicated;
    SC_TRY(duplicateConnectedSocket(rawAcceptedDescriptor, options.connectionOptions, duplicated));
    outConnection = move(duplicated);
    return Result(true);
}

SC::ResultFile SC::NamedPipeServer::close()
{
    if (not created)
    {
        return Result(true);
    }
    created = false;

    const ResultFile closeResult = listeningSocket.close();

    if (options.posix.removeEndpointOnClose and not name.isEmpty())
    {
        int unlinkResult;
        do
        {
            unlinkResult = ::unlink(name.view().bytesIncludingTerminator());
        } while (unlinkResult == -1 and errno == EINTR);
        if (unlinkResult != 0 and errno != ENOENT)
            return ResultFile::withNativeError(FileError::EndpointCleanupFailed,
                                               FileErrorDetail::RemoveNamedPipeEndpoint, static_cast<uint32_t>(errno));
    }
    return closeResult;
}

SC::ResultFile SC::NamedPipeClient::connect(StringSpan pipeName, PipeDescriptor& outConnection,
                                            NamedPipeClientOptions options)
{
    if (pipeName.getEncoding() == StringEncoding::Utf16)
        return {FileError::UnsupportedPathEncoding, FileErrorDetail::ValidateNamedPipePath};

    StringPath nullTerminatedPath;
    if (not nullTerminatedPath.assign(pipeName))
        return {FileError::PathCapacityExceeded, FileErrorDetail::BuildNamedPipePath};
    const char* fullPath = nullTerminatedPath.view().bytesIncludingTerminator();
    if (fullPath[0] != '/')
        return {FileError::PathMustBeAbsolute, FileErrorDetail::ValidateAbsolutePath};
    sockaddr_un sizeCheck;
    if (::strlen(fullPath) >= sizeof(sizeCheck.sun_path))
        return {FileError::PathCapacityExceeded, FileErrorDetail::ValidateNamedPipePath};

    int socketDescriptor;
    do
    {
        socketDescriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
    } while (socketDescriptor == -1 and errno == EINTR);
    if (socketDescriptor == -1)
        return ResultFile::withNativeError(FileError::NamedPipeConnectionFailed,
                                           FileErrorDetail::ConnectNamedPipeClient, static_cast<uint32_t>(errno));

    FileDescriptor connectedSocket;
    if (not connectedSocket.assign(socketDescriptor))
    {
        ::close(socketDescriptor);
        return {FileError::InvalidState, FileErrorDetail::ConnectNamedPipeClient};
    }

    sockaddr_un address;
    ::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    ::memcpy(address.sun_path, fullPath, ::strlen(fullPath) + 1);

    int connectResult;
    do
    {
        connectResult = ::connect(socketDescriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    } while (connectResult == -1 and errno == EINTR);
    if (connectResult != 0)
        return ResultFile::withNativeError(FileError::NamedPipeConnectionFailed,
                                           FileErrorDetail::ConnectNamedPipeClient, static_cast<uint32_t>(errno));

    int rawConnectedDescriptor;
    if (not connectedSocket.get(rawConnectedDescriptor, false))
        return {FileError::InvalidHandle, FileErrorDetail::ConnectNamedPipeClient};

    PipeDescriptor duplicated;
    SC_TRY(duplicateConnectedSocket(rawConnectedDescriptor, options.connectionOptions, duplicated));
    outConnection = move(duplicated);
    return Result(true);
}

#endif
