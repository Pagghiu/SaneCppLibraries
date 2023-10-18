// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Strings/SmallString.h"
#include "../System/Time.h"

namespace SC
{
struct FileSystem;
struct StringConverter;
} // namespace SC

struct SC::FileSystem
{
    StringNative<512> currentDirectory = StringEncoding::Native;

    bool localizedErrorMessages = false;
    bool preciseErrorMessages   = false;

    Result init(StringView currentWorkingDirectory);
    Result changeDirectory(StringView currentWorkingDirectory);

    struct CopyFlags
    {
        CopyFlags()
        {
            overwrite           = false;
            useCloneIfSupported = true;
        }

        bool       overwrite;
        CopyFlags& setOverwrite(bool value)
        {
            overwrite = value;
            return *this;
        }

        bool       useCloneIfSupported;
        CopyFlags& setUseCloneIfSupported(bool value)
        {
            useCloneIfSupported = value;
            return *this;
        }
    };

    struct CopyOperation
    {
        StringView source;
        StringView destination;
        CopyFlags  copyFlags;
    };
    [[nodiscard]] Result copyFile(Span<const CopyOperation> sourceDestination);
    [[nodiscard]] Result copyFile(StringView source, StringView destination, CopyFlags copyFlags = CopyFlags())
    {
        return copyFile(CopyOperation{source, destination, copyFlags});
    }
    [[nodiscard]] Result copyDirectory(Span<const CopyOperation> sourceDestination);
    [[nodiscard]] Result copyDirectory(StringView source, StringView destination, CopyFlags copyFlags = CopyFlags())
    {
        return copyDirectory(CopyOperation{source, destination, copyFlags});
    }
    [[nodiscard]] Result removeFile(Span<const StringView> files);
    [[nodiscard]] Result removeFile(StringView source) { return removeFile(Span<const StringView>{source}); }
    [[nodiscard]] Result removeFileIfExists(StringView source);
    [[nodiscard]] Result removeDirectoryRecursive(Span<const StringView> directories);
    [[nodiscard]] Result removeEmptyDirectory(Span<const StringView> directories);
    [[nodiscard]] Result removeEmptyDirectoryRecursive(Span<const StringView> directories);
    [[nodiscard]] Result makeDirectory(Span<const StringView> directories);
    [[nodiscard]] Result makeDirectoryRecursive(Span<const StringView> directories);
    [[nodiscard]] Result makeDirectoryIfNotExists(Span<const StringView> directories);
    [[nodiscard]] bool   exists(StringView fileOrDirectory);
    [[nodiscard]] bool   existsAndIsDirectory(StringView directory);
    [[nodiscard]] bool   existsAndIsFile(StringView file);
    [[nodiscard]] Result write(StringView file, Span<const char> data);
    [[nodiscard]] Result read(StringView file, Vector<char>& data);
    [[nodiscard]] Result write(StringView file, StringView text);
    [[nodiscard]] Result read(StringView file, String& data, StringEncoding encoding);

    struct FileTime
    {
        AbsoluteTime modifiedTime = 0;
    };
    [[nodiscard]] Result getFileTime(StringView file, FileTime& fileTime);
    [[nodiscard]] Result setLastModifiedTime(StringView file, AbsoluteTime time);

  private:
    [[nodiscard]] bool convert(const StringView file, String& destination, StringView* encodedPath = nullptr);

    StringNative<128> fileFormatBuffer1  = StringEncoding::Native;
    StringNative<128> fileFormatBuffer2  = StringEncoding::Native;
    StringNative<128> errorMessageBuffer = StringEncoding::Native;

    Result formatError(int errorNumber, StringView item, bool isWindowsNativeError);
    struct Internal;
};
