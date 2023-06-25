// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystem.h"
#include "../Foundation/StringConverter.h"

#if SC_PLATFORM_WINDOWS
#include "FileSystemInternalWindows.inl"
#include "UtilityWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "FileSystemInternalEmscripten.inl"
#else
#include "FileSystemInternalPosix.inl"
#endif

#include "../FileSystem/Path.h"

SC::ReturnCode SC::FileSystem::init(StringView currentWorkingDirectory)
{
    return changeDirectory(currentWorkingDirectory);
}

SC::ReturnCode SC::FileSystem::changeDirectory(StringView currentWorkingDirectory)
{
    StringConverter converter(currentDirectory);
    converter.clear();
    SC_TRY_IF(converter.appendNullTerminated(currentWorkingDirectory));
    // TODO: Assert if path is not absolute
    return existsAndIsDirectory(".");
}

bool SC::FileSystem::convert(const StringView file, String& destination, StringView* encodedPath)
{
    StringConverter converter(destination);
    if (Path::isAbsolute(file))
    {
        if (encodedPath != nullptr)
        {
            return converter.convertNullTerminateFastPath(file, *encodedPath);
        }
        converter.clear();
        return converter.appendNullTerminated(file);
    }
    if (currentDirectory.isEmpty())
        return false;
    converter.clear();
    SC_TRY_IF(converter.appendNullTerminated(currentDirectory.view()));
#if SC_PLATFORM_WINDOWS
    SC_TRY_IF(converter.appendNullTerminated(L"\\"));
#else
    SC_TRY_IF(converter.appendNullTerminated("/"));
#endif
    SC_TRY_IF(converter.appendNullTerminated(file));
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
    SC_TRY_IF(convert(path, fileFormatBuffer1, &encodedPath));
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
    SC_TRY_IF(convert(path, fileFormatBuffer1, &encodedPath));
    FILE* handle;
    SC_TRY_FORMAT_ERRNO(path, Internal::openFileRead(encodedPath.getNullTerminatedNative(), handle));
    SC_TRY_FORMAT_LIBC(fseek(handle, 0, SEEK_END));
    const auto fileSizeRes = ftell(handle);
    SC_TRY_FORMAT_LIBC(fileSizeRes);
    SC_TRY_FORMAT_LIBC(fseek(handle, 0, SEEK_SET));
    const size_t fileSize = static_cast<size_t>(fileSizeRes);
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
        if (not UtilityWindows::formatWindowsError(errorNumber, errorMessageBuffer))
        {
            return "SC::FileSystem::formatError - Cannot format error"_a8;
        }
    }
    else
#endif
    {
        SC_UNUSED(isWindowsNativeError);
        if (not preciseErrorMessages)
        {
            return getErrorCode(errorNumber);
        }

        if (not Internal::formatError(errorNumber, errorMessageBuffer))
        {
            return "SC::FileSystem::formatError - Cannot format error"_a8;
        }
    }
    StringConverter errorMessage(errorMessageBuffer);
    SC_TRY_IF(errorMessage.appendNullTerminated(" for \""));
    SC_TRY_IF(errorMessage.appendNullTerminated(item));
    SC_TRY_IF(errorMessage.appendNullTerminated("\""));
    return errorMessageBuffer.view();
}

SC::ReturnCode SC::FileSystem::removeFile(Span<const StringView> files)
{
    StringView encodedPath;
    for (auto& path : files)
    {
        SC_TRY_IF(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, Internal::removeFile(encodedPath.getNullTerminatedNative()));
    }
    return true;
}

SC::ReturnCode SC::FileSystem::removeDirectoryRecursive(Span<const StringView> directories)
{
    for (auto& path : directories)
    {
        SC_TRY_IF(convert(path, fileFormatBuffer1)); // force write
        SC_TRY_FORMAT_ERRNO(path, Internal::removeDirectoryRecursive(fileFormatBuffer1));
    }
    return true;
}

SC::ReturnCode SC::FileSystem::copyFile(Span<const CopyOperation> sourceDestination)
{
    if (currentDirectory.isEmpty())
        return false;
    StringView encodedPath1, encodedPath2;
    for (const CopyOperation& op : sourceDestination)
    {
        SC_TRY_IF(convert(op.source, fileFormatBuffer1, &encodedPath1));
        SC_TRY_IF(convert(op.destination, fileFormatBuffer2, &encodedPath2));
        SC_TRY_FORMAT_NATIVE(op.source, Internal::copyFile(encodedPath1, encodedPath2, op.copyFlags));
    }
    return true;
}

SC::ReturnCode SC::FileSystem::copyDirectory(Span<const CopyOperation> sourceDestination)
{
    if (currentDirectory.isEmpty())
        return false;
    for (const CopyOperation& op : sourceDestination)
    {
        SC_TRY_IF(convert(op.source, fileFormatBuffer1));      // force write
        SC_TRY_IF(convert(op.destination, fileFormatBuffer2)); // force write
        SC_TRY_FORMAT_NATIVE(op.source, Internal::copyDirectory(fileFormatBuffer1, fileFormatBuffer2, op.copyFlags));
    }
    return true;
}

SC::ReturnCode SC::FileSystem::removeEmptyDirectory(Span<const StringView> directories)
{
    StringView encodedPath;
    for (StringView path : directories)
    {
        SC_TRY_IF(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, Internal::removeEmptyDirectory(encodedPath.getNullTerminatedNative()));
    }
    return true;
}

SC::ReturnCode SC::FileSystem::makeDirectory(Span<const StringView> directories)
{
    StringView encodedPath;
    for (auto& path : directories)
    {
        SC_TRY_IF(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, Internal::makeDirectory(encodedPath.getNullTerminatedNative()));
    }
    return true;
}

[[nodiscard]] bool SC::FileSystem::exists(StringView fileOrDirectory)
{
    StringView encodedPath;
    SC_TRY_IF(convert(fileOrDirectory, fileFormatBuffer1, &encodedPath));
    return Internal::exists(encodedPath.getNullTerminatedNative());
}

bool SC::FileSystem::existsAndIsDirectory(StringView directory)
{
    StringView encodedPath;
    SC_TRY_IF(convert(directory, fileFormatBuffer1, &encodedPath));
    return Internal::existsAndIsDirectory(encodedPath.getNullTerminatedNative());
}

[[nodiscard]] bool SC::FileSystem::existsAndIsFile(StringView file)
{
    StringView encodedPath;
    SC_TRY_IF(convert(file, fileFormatBuffer1, &encodedPath));
    return Internal::existsAndIsFile(encodedPath.getNullTerminatedNative());
}

SC::Optional<SC::FileSystem::FileTime> SC::FileSystem::getFileTime(StringView file)
{
    StringView encodedPath;
    if (convert(file, fileFormatBuffer1, &encodedPath))
    {
        FileTime time;
        if (Internal::getFileTime(encodedPath.getNullTerminatedNative(), time))
        {
            return time;
        }
    }
    return {};
}

SC::ReturnCode SC::FileSystem::setLastModifiedTime(StringView file, AbsoluteTime time)
{
    StringView encodedPath;
    SC_TRY_IF(convert(file, fileFormatBuffer1, &encodedPath));
    return Internal::setLastModifiedTime(encodedPath.getNullTerminatedNative(), time);
}

#undef SC_TRY_FORMAT_ERRNO
