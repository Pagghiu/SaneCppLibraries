// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystem.h"

#if SC_PLATFORM_WINDOWS
#include "FileSystemInternalWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "FileSystemInternalEmscripten.inl"
#else
#include "FileSystemInternalPosix.inl"
#endif

#include "../Foundation/Path.h"

SC::ReturnCode SC::FileSystem::init(StringView currentWorkingDirectory)
{
    return changeDirectory(currentWorkingDirectory);
}

SC::ReturnCode SC::FileSystem::changeDirectory(StringView currentWorkingDirectory)
{
    currentDirectory.clear();
    SC_TRY_IF(currentDirectory.appendNullTerminated(currentWorkingDirectory));
    // TODO: Assert if path is not absolute
    return true;
}

void SC::FileSystem::close() {}
// TODO: We should not need to do conversion all the time. Maybe do a push/pop
// TODO: We should also combine paths using paths api to resolve relative paths
bool SC::FileSystem::pushFile1(StringView file, StringView& encodedPath)
{
    if (Path::isAbsolute(file))
    {
        return fileFormatBuffer1.convertNullTerminateFastPath(file, encodedPath);
    }
    if (currentDirectory.text.isEmpty())
        return false;
    SC_TRY_IF(fileFormatBuffer1.text.assign(currentDirectory.text.view()));
#if SC_PLATFORM_WINDOWS
    SC_TRY_IF(fileFormatBuffer1.appendNullTerminated(L"\\"));
#else
    SC_TRY_IF(fileFormatBuffer1.appendNullTerminated("/"));
#endif
    SC_TRY_IF(fileFormatBuffer1.appendNullTerminated(file));
    encodedPath = fileFormatBuffer1.view();
    return true;
}

bool SC::FileSystem::pushFile2(StringView file, StringView& encodedPath)
{
    if (Path::isAbsolute(file))
    {
        return fileFormatBuffer2.convertNullTerminateFastPath(file, encodedPath);
    }
    if (currentDirectory.text.isEmpty())
        return false;
    SC_TRY_IF(fileFormatBuffer2.text.assign(currentDirectory.text.view()));
#if SC_PLATFORM_WINDOWS
    SC_TRY_IF(fileFormatBuffer2.appendNullTerminated(L"\\"));
#else
    SC_TRY_IF(fileFormatBuffer2.appendNullTerminated("/"));
#endif
    SC_TRY_IF(fileFormatBuffer2.appendNullTerminated(file));
    encodedPath = fileFormatBuffer2.view();
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
            if (IsSame<decltype(tempRes), ReturnCode>::value)                                                          \
            {                                                                                                          \
                return tempRes;                                                                                        \
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
            if (IsSame<decltype(tempRes), ReturnCode>::value)                                                          \
            {                                                                                                          \
                return tempRes;                                                                                        \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                return formatError(errno, path, false);                                                                \
            }                                                                                                          \
        }                                                                                                              \
    }
#endif
SC::ReturnCode SC::FileSystem::write(StringView path, SpanVoid<const void> data)
{
    StringView encodedPath;
    SC_TRY_IF(pushFile1(path, encodedPath));
    FILE* handle = nullptr;
    SC_TRY_FORMAT_ERRNO(path, Internal::openFileWrite(encodedPath.getNullTerminatedNative(), handle));
    const size_t res = fwrite(data.data(), 1, data.sizeInBytes(), handle);
    if (ferror(handle) != 0)
    {
        return formatError(errno, path, false);
    }
    fclose(handle);
    return res == data.sizeInBytes();
}

#define SC_TRY_FORMAT_LIBC(func)                                                                                       \
    {                                                                                                                  \
        if (func == -1)                                                                                                \
        {                                                                                                              \
            return formatError(errno, path, false);                                                                    \
        }                                                                                                              \
    }
SC::ReturnCode SC::FileSystem::read(StringView path, Vector<char>& data)
{
    StringView encodedPath;
    SC_TRY_IF(pushFile1(path, encodedPath));
    FILE* handle;
    SC_TRY_FORMAT_ERRNO(path, Internal::openFileRead(encodedPath.getNullTerminatedNative(), handle));
    SC_TRY_FORMAT_LIBC(fseek(handle, 0, SEEK_END));
    const auto fileSize = ftell(handle);
    SC_TRY_FORMAT_LIBC(fileSize);
    SC_TRY_FORMAT_LIBC(fseek(handle, 0, SEEK_SET));
    SC_TRY_IF(data.resizeWithoutInitializing(fileSize));

    const auto readBytes = fread(data.data(), 1, fileSize, handle);
    if (ferror(handle) != 0)
    {
        return formatError(errno, path, false);
    }
    fclose(handle);
    return readBytes == fileSize;
}
#undef SC_TRY_FORMAT_LIBC

[[nodiscard]] SC::ReturnCode SC::FileSystem::write(StringView file, StringView text)
{
    return write(file, text.toVoidSpan());
}

[[nodiscard]] SC::ReturnCode SC::FileSystem::read(StringView file, String& text, StringEncoding encoding)
{
    text.encoding = encoding;
    SC_TRY_IF(read(file, text.data));
    return text.pushNullTerm();
}

SC::ReturnCode SC::FileSystem::formatError(int errorNumber, StringView item, bool isWindowsNativeError)
{
#if SC_PLATFORM_WINDOWS
    if (isWindowsNativeError)
    {
        if (not preciseErrorMessages)
        {
            return "Windows Error"_a8;
        }
        if (not Internal::formatWindowsError(errorNumber, errorMessageBuffer.text))
        {
            return "SC::FileSystem::formatError - Cannot format error"_a8;
        }
    }
    else
#endif
    {
        if (not preciseErrorMessages)
        {
            return getErrorCode(errorNumber);
        }

        if (not Internal::formatError(errorNumber, errorMessageBuffer.text))
        {
            return "SC::FileSystem::formatError - Cannot format error"_a8;
        }
    }
    SC_TRY_IF(errorMessageBuffer.appendNullTerminated(" for \""));
    SC_TRY_IF(errorMessageBuffer.appendNullTerminated(item));
    SC_TRY_IF(errorMessageBuffer.appendNullTerminated("\""));
    return errorMessageBuffer.view();
}

SC::ReturnCode SC::FileSystem::removeFile(Span<const StringView> files)
{
    StringView encodedPath;
    for (auto& path : files)
    {
        SC_TRY_IF(pushFile1(path, encodedPath));
        SC_TRY_FORMAT_ERRNO(path, Internal::removeFile(encodedPath.getNullTerminatedNative()));
    }
    return true;
}

SC::ReturnCode SC::FileSystem::removeDirectoryRecursive(Span<const StringView> directories)
{
    StringView encodedPath;
    for (auto& path : directories)
    {
        SC_TRY_IF(pushFile1(path, encodedPath));
        SC_TRY_FORMAT_ERRNO(path, Internal::removeDirectoryRecursive(fileFormatBuffer1));
    }
    return true;
}

SC::ReturnCode SC::FileSystem::copyFile(Span<const CopyOperation> sourceDestination)
{
    if (currentDirectory.text.isEmpty())
        return false;
    StringView encodedPath1, encodedPath2;
    for (const CopyOperation& op : sourceDestination)
    {
        SC_TRY_IF(pushFile1(op.source, encodedPath1));
        SC_TRY_IF(pushFile2(op.destination, encodedPath2));
        SC_TRY_FORMAT_NATIVE(op.source,
                             Internal::copyFile(fileFormatBuffer1.view().getNullTerminatedNative(),
                                                fileFormatBuffer2.view().getNullTerminatedNative(), op.copyFlags));
    }
    return true;
}

SC::ReturnCode SC::FileSystem::copyDirectory(Span<const CopyOperation> sourceDestination)
{
    if (currentDirectory.text.isEmpty())
        return false;
    StringView encodedPath1, encodedPath2;
    for (const CopyOperation& op : sourceDestination)
    {
        SC_TRY_IF(pushFile1(op.source, encodedPath1));
        SC_TRY_IF(pushFile2(op.destination, encodedPath2));
        SC_TRY_FORMAT_NATIVE(op.source, Internal::copyDirectory(fileFormatBuffer1, fileFormatBuffer2, op.copyFlags));
    }
    return true;
}

SC::ReturnCode SC::FileSystem::removeEmptyDirectory(Span<const StringView> directories)
{
    StringView encodedPath;
    for (StringView path : directories)
    {
        SC_TRY_IF(pushFile1(path, encodedPath));
        SC_TRY_FORMAT_ERRNO(path, Internal::removeEmptyDirectory(encodedPath.getNullTerminatedNative()));
    }
    return true;
}

SC::ReturnCode SC::FileSystem::makeDirectory(Span<const StringView> directories)
{
    StringView encodedPath;
    for (auto& path : directories)
    {
        SC_TRY_IF(pushFile1(path, encodedPath));
        SC_TRY_FORMAT_ERRNO(path, Internal::makeDirectory(encodedPath.getNullTerminatedNative()));
    }
    return true;
}

[[nodiscard]] bool SC::FileSystem::exists(StringView fileOrDirectory)
{
    StringView encodedPath;
    SC_TRY_IF(pushFile1(fileOrDirectory, encodedPath));
    return Internal::exists(encodedPath.getNullTerminatedNative());
}

bool SC::FileSystem::existsAndIsDirectory(StringView directory)
{
    StringView encodedPath;
    SC_TRY_IF(pushFile1(directory, encodedPath));
    return Internal::existsAndIsDirectory(encodedPath.getNullTerminatedNative());
}

[[nodiscard]] bool SC::FileSystem::existsAndIsFile(StringView file)
{
    StringView encodedPath;
    SC_TRY_IF(pushFile1(file, encodedPath));
    return Internal::existsAndIsFile(encodedPath.getNullTerminatedNative());
}

#undef SC_TRY_FORMAT_ERRNO
