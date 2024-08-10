// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "FileSystem.h"
#include "../File/FileDescriptor.h"
#include "../Strings/StringConverter.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/FileSystemWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "Internal/FileSystemEmscripten.inl"
#else
#include "Internal/FileSystemPosix.inl"
#endif

#include "../FileSystem/Path.h"

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
    FileDescriptor file;
    SC_TRY(file.open(encodedPath, FileDescriptor::WriteCreateTruncate));
    return file.write(data);
}

SC::Result SC::FileSystem::write(StringView path, Span<const uint8_t> data)
{
    return write(path, {reinterpret_cast<const char*>(data.data()), data.sizeInBytes()});
}

SC::Result SC::FileSystem::read(StringView path, Vector<char>& data)
{
    StringView encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
    FileDescriptor file;
    SC_TRY(file.open(encodedPath, FileDescriptor::ReadOnly));
    return file.readUntilEOF(data);
}

SC::Result SC::FileSystem::read(StringView path, Vector<uint8_t>& data)
{
    StringView encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
    FileDescriptor file;
    SC_TRY(file.open(encodedPath, FileDescriptor::ReadOnly));
    return file.readUntilEOF(data);
}

[[nodiscard]] SC::Result SC::FileSystem::writeString(StringView path, StringView text)
{
    return write(path, text.toCharSpan());
}

[[nodiscard]] SC::Result SC::FileSystem::writeStringAppend(StringView path, StringView text)
{
    StringView encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
    FileDescriptor file;
    SC_TRY(file.open(encodedPath, FileDescriptor::WriteAppend));
    return file.write(text.toCharSpan());
}

[[nodiscard]] SC::Result SC::FileSystem::read(StringView path, String& text, StringEncoding encoding)
{
    text.data.clearWithoutInitializing();
    text.encoding = encoding;
    StringView encodedPath;
    SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
    FileDescriptor file;
    SC_TRY(file.open(encodedPath, FileDescriptor::ReadOnly));
    return file.readUntilEOF(text);
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
        if (not UtilityWindows::formatWindowsError(errorNumber, errorMessageBuffer))
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

SC::Result SC::FileSystem::removeFiles(Span<const StringView> files)
{
    StringView encodedPath;
    for (auto& path : files)
    {
        SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, Internal::removeFile(encodedPath.getNullTerminatedNative()));
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
#if SC_PLATFORM_APPLE || SC_FILESYSTEM_WINDOWS_USE_SHELL_OPERATIONS
        SC_TRY_FORMAT_ERRNO(path, Internal::removeDirectoryRecursive(fileFormatBuffer1));
#else
        SC_TRY_FORMAT_ERRNO(path, fallbackRemoveDirectoryRecursive(fileFormatBuffer1));
#endif
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
        SC_TRY_FORMAT_NATIVE(op.source, Internal::copyFile(encodedPath1, encodedPath2, op.copyFlags));
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
#if SC_PLATFORM_APPLE || SC_FILESYSTEM_WINDOWS_USE_SHELL_OPERATIONS
        SC_TRY_FORMAT_NATIVE(op.source, Internal::copyDirectory(fileFormatBuffer1, fileFormatBuffer2, op.copyFlags));
#else
        SC_TRY_FORMAT_NATIVE(op.source, fallbackCopyDirectory(fileFormatBuffer1, fileFormatBuffer2, op.copyFlags));
#endif
    }
    return Result(true);
}

SC::Result SC::FileSystem::removeEmptyDirectories(Span<const StringView> directories)
{
    StringView encodedPath;
    for (StringView path : directories)
    {
        SC_TRY(convert(path, fileFormatBuffer1, &encodedPath));
        SC_TRY_FORMAT_ERRNO(path, Internal::removeEmptyDirectory(encodedPath.getNullTerminatedNative()));
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
        while (Internal::removeEmptyDirectory(encodedPath.getNullTerminatedNative()))
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
        SC_TRY_FORMAT_ERRNO(path, Internal::makeDirectory(encodedPath.getNullTerminatedNative()));
    }
    return Result(true);
}

SC::Result SC::FileSystem::makeDirectoriesRecursive(Span<const StringView> directories)
{
    StringView encodedPath;
    for (auto& path : directories)
    {
        SC_TRY(convert(path, fileFormatBuffer2, &encodedPath));
        StringView dirnameDirectory = encodedPath;
        int        levelsToCreate   = 0;
        while (not existsAndIsDirectory(dirnameDirectory) or dirnameDirectory.isEmpty())
        {
            dirnameDirectory = Path::dirname(dirnameDirectory, Path::AsNative);
            levelsToCreate++;
        }
        for (int idx = 0; idx < levelsToCreate; ++idx)
        {
            if (levelsToCreate - idx - 2 >= 0)
            {
                StringView partialPath = Path::dirname(encodedPath, Path::AsNative, levelsToCreate - idx - 2);
                SC_TRY(convert(partialPath, fileFormatBuffer1, &dirnameDirectory));
            }
            else
            {
                dirnameDirectory = encodedPath;
            }
            SC_TRY_FORMAT_ERRNO(path, Internal::makeDirectory(dirnameDirectory.getNullTerminatedNative()));
        }
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
    SC_TRY(Internal::createSymbolicLink(sourceFileNative.getNullTerminatedNative(),
                                        linkFileNative.getNullTerminatedNative()));
    return Result(true);
}

[[nodiscard]] bool SC::FileSystem::exists(StringView fileOrDirectory)
{
    StringView encodedPath;
    SC_TRY(convert(fileOrDirectory, fileFormatBuffer1, &encodedPath));
    return Internal::exists(encodedPath.getNullTerminatedNative());
}

bool SC::FileSystem::existsAndIsDirectory(StringView directory)
{
    StringView encodedPath;
    SC_TRY(convert(directory, fileFormatBuffer1, &encodedPath));
    return Internal::existsAndIsDirectory(encodedPath.getNullTerminatedNative());
}

[[nodiscard]] bool SC::FileSystem::existsAndIsFile(StringView file)
{
    StringView encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    return Internal::existsAndIsFile(encodedPath.getNullTerminatedNative());
}

[[nodiscard]] bool SC::FileSystem::existsAndIsLink(StringView file)
{
    StringView encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    return Internal::existsAndIsLink(encodedPath.getNullTerminatedNative());
}

[[nodiscard]] bool SC::FileSystem::moveDirectory(StringView sourceDirectory, StringView destinationDirectory)
{
    StringView encodedPath1;
    StringView encodedPath2;
    SC_TRY(convert(sourceDirectory, fileFormatBuffer1, &encodedPath1));
    SC_TRY(convert(destinationDirectory, fileFormatBuffer2, &encodedPath2));
    return Internal::moveDirectory(encodedPath1.getNullTerminatedNative(), encodedPath2.getNullTerminatedNative());
}

SC::Result SC::FileSystem::getFileStat(StringView file, FileStat& fileTime)
{
    StringView encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    return Internal::getFileStat(encodedPath.getNullTerminatedNative(), fileTime);
}

SC::Result SC::FileSystem::setLastModifiedTime(StringView file, Time::Absolute time)
{
    StringView encodedPath;
    SC_TRY(convert(file, fileFormatBuffer1, &encodedPath));
    return Internal::setLastModifiedTime(encodedPath.getNullTerminatedNative(), time);
}

#if !SC_PLATFORM_APPLE
#include "../Containers/SmallVector.h"
#include "../FileSystemIterator/FileSystemIterator.h"
SC::Result SC::FileSystem::fallbackCopyDirectory(String& sourceDirectory, String& destinationDirectory,
                                                 FileSystem::CopyFlags options)
{
    FileSystemIterator fileIterator;
    StringView         sourceView = sourceDirectory.view();
    SC_TRY(fileIterator.init(sourceView));
    fileIterator.options.recursive           = true;
    StringNative<512> destinationPath        = StringEncoding::Native;
    StringNative<512> destinationPathDirname = StringEncoding::Native;
    if (not Internal::existsAndIsDirectory(destinationDirectory.view().getNullTerminatedNative()))
    {
        SC_TRY(Internal::makeDirectory(destinationDirectory.view().getNullTerminatedNative()));
    }
    while (fileIterator.enumerateNext())
    {
        const FileSystemIterator::Entry& entry = fileIterator.get();

        const StringView partialPath = entry.path.sliceStartBytes(sourceView.sizeInBytes());
        StringConverter  destinationConvert(destinationPath, StringConverter::Clear);
        SC_TRY(destinationConvert.appendNullTerminated(destinationDirectory.view()));
        SC_TRY(destinationConvert.appendNullTerminated(partialPath));
        if (entry.isDirectory())
        {
            if (options.overwrite)
            {
                // We don't care about the result
                if (Internal::existsAndIsFile(destinationPath.view().getNullTerminatedNative()))
                {
                    (void)Internal::removeFile(destinationPath.view().getNullTerminatedNative());
                }
            }
            // The directory may have been already been created by a file copy
            if (not Internal::existsAndIsDirectory(destinationPath.view().getNullTerminatedNative()))
            {
                SC_TRY(Internal::makeDirectory(destinationPath.view().getNullTerminatedNative()));
            }
        }
        else
        {
            // When creating a file the directory may have not been already created (happens on Linux)
            destinationPathDirname = Path::dirname(destinationDirectory.view(), Path::AsNative);
            if (not Internal::existsAndIsDirectory(destinationPathDirname.view().getNullTerminatedNative()))
            {
                SC_TRY(Internal::makeDirectory(destinationPathDirname.view().getNullTerminatedNative()));
            }
            SC_TRY(Internal::copyFile(entry.path, destinationPath.view(), options));
        }
    }
    SC_TRY(fileIterator.checkErrors());
    return Result(true);
}

SC::Result SC::FileSystem::fallbackRemoveDirectoryRecursive(String& sourceDirectory)
{
    FileSystemIterator fileIterator;
    SC_TRY(fileIterator.init(sourceDirectory.view()));
    fileIterator.options.recursive = true;
    SmallVector<StringNative<512>, 64> emptyDirectories;
    while (fileIterator.enumerateNext())
    {
        const FileSystemIterator::Entry& entry = fileIterator.get();
        if (entry.isDirectory())
        {
            SC_TRY(emptyDirectories.push_back(entry.path));
        }
        else
        {
            SC_TRY(Internal::removeFile(entry.path.getNullTerminatedNative()));
        }
    }
    SC_TRY(fileIterator.checkErrors());

    while (not emptyDirectories.isEmpty())
    {
        SC_TRY(Internal::removeEmptyDirectory(emptyDirectories.back().view().getNullTerminatedNative()));
        SC_TRY(emptyDirectories.pop_back());
    }
    SC_TRY(Internal::removeEmptyDirectory(sourceDirectory.view().getNullTerminatedNative()));
    return Result(true);
}
#endif

#undef SC_TRY_FORMAT_ERRNO
